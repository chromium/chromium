// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_controller.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/hash/sha1.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

// TODO(b/161357364): refactor utility functions and constants

constexpr net::BackoffEntry::Policy kFetchTopicRetryBackoffPolicy = {
    10,             // Number of initial errors to ignore.
    500,            // Initial delay in ms.
    2.0,            // Factor by which the waiting time will be multiplied.
    0.2,            // Fuzzing percentage.
    2 * 60 * 1000,  // Maximum delay in ms.
    -1,             // Never discard the entry.
    true,           // Use initial delay.
};

constexpr net::BackoffEntry::Policy kResumeFetchImageBackoffPolicy = {
    kMaxConsecutiveReadPhotoFailures,  // Number of initial errors to ignore.
    500,                               // Initial delay in ms.
    2.0,            // Factor by which the waiting time will be multiplied.
    0.2,            // Fuzzing percentage.
    8 * 60 * 1000,  // Maximum delay in ms.
    -1,             // Never discard the entry.
    true,           // Use initial delay.
};

using DownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

void DownloadImageFromUrl(const std::string& url, DownloadCallback callback) {
  DCHECK(!url.empty());

  // During shutdown, we may not have `ImageDownloader` when reach here.
  if (!ImageDownloader::Get())
    return;

  ImageDownloader::Get()->Download(GURL(url), NO_TRAFFIC_ANNOTATION_YET,
                                   base::BindOnce(std::move(callback)));
}

void ToImageSkia(DownloadCallback callback, const SkBitmap& image) {
  if (image.isNull()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(image);
  image_skia.MakeThreadSafe();

  std::move(callback).Run(image_skia);
}

base::TaskTraits GetTaskTraits() {
  return {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
}

// Get the root path for ambient mode.
base::FilePath GetRootPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));
}

base::FilePath GetCachePath() {
  return GetRootPath().Append(
      FILE_PATH_LITERAL(kAmbientModeCacheDirectoryName));
}

base::FilePath GetBackupCachePath() {
  return GetRootPath().Append(
      FILE_PATH_LITERAL(kAmbientModeBackupCacheDirectoryName));
}

base::FilePath GetBackupFilePath(size_t index) {
  return GetBackupCachePath().Append(base::NumberToString(index) +
                                     kPhotoFileExt);
}

bool CreateDirIfNotExists(const base::FilePath& path) {
  return base::DirectoryExists(path) || base::CreateDirectory(path);
}

void WriteFile(const base::FilePath& path, const std::string& data) {
  if (!CreateDirIfNotExists(GetCachePath())) {
    LOG(ERROR) << "Cannot create ambient mode directory.";
    return;
  }

  if (base::SysInfo::AmountOfFreeDiskSpace(GetRootPath()) <
      kMaxReservedAvailableDiskSpaceByte) {
    LOG(WARNING) << "Not enough disk space left.";
    return;
  }

  // Create a temp file.
  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(path.DirName(), &temp_file)) {
    LOG(ERROR) << "Cannot create a temporary file.";
    return;
  }

  // Write to the tmp file.
  const int size = data.size();
  int written_size = base::WriteFile(temp_file, data.data(), size);
  if (written_size != size) {
    LOG(ERROR) << "Cannot write the temporary file.";
    base::DeleteFile(temp_file);
    return;
  }

  // Replace the current file with the temp file.
  if (!base::ReplaceFile(temp_file, path, /*error=*/nullptr))
    LOG(ERROR) << "Cannot replace the temporary file.";
}

const std::array<const char*, 2>& GetBackupPhotoUrls() {
  return Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->GetBackupPhotoUrls();
}

}  // namespace

class AmbientURLLoaderImpl : public AmbientURLLoader {
 public:
  AmbientURLLoaderImpl()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}
  ~AmbientURLLoaderImpl() override = default;

  // AmbientURLLoader:
  void Download(
      const std::string& url,
      network::SimpleURLLoader::BodyAsStringCallback callback) override {
    auto simple_loader = CreateSimpleURLLoader(url);
    auto* loader_ptr = simple_loader.get();
    auto loader_factory = AmbientClient::Get()->GetURLLoaderFactory();
    loader_ptr->DownloadToString(
        loader_factory.get(),
        base::BindOnce(&AmbientURLLoaderImpl::OnUrlDownloaded,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(simple_loader), loader_factory),
        kMaxImageSizeInBytes);
  }

  void DownloadToFile(
      const std::string& url,
      network::SimpleURLLoader::DownloadToFileCompleteCallback callback,
      const base::FilePath& file_path) override {
    auto simple_loader = CreateSimpleURLLoader(url);
    auto loader_factory = AmbientClient::Get()->GetURLLoaderFactory();
    auto* loader_ptr = simple_loader.get();

    // Create a temporary file path as target for download to guard against race
    // conditions in reading.
    base::FilePath temp_path = file_path.DirName().Append(
        base::UnguessableToken::Create().ToString() + kPhotoFileExt);

    // Download to temp file first to guarantee entire image is written without
    // errors before attempting to read it.
    loader_ptr->DownloadToFile(
        loader_factory.get(),
        base::BindOnce(&AmbientURLLoaderImpl::OnUrlDownloadedToFile,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(simple_loader), std::move(loader_factory),
                       file_path),
        temp_path);
  }

 private:
  std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(
      const std::string& url) {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = GURL(url);
    resource_request->method = "GET";
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

    return network::SimpleURLLoader::Create(std::move(resource_request),
                                            NO_TRAFFIC_ANNOTATION_YET);
  }

  // Called when the download completes.
  void OnUrlDownloaded(
      network::SimpleURLLoader::BodyAsStringCallback callback,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      std::unique_ptr<std::string> response_body) {
    if (simple_loader->NetError() == net::OK && response_body) {
      std::move(callback).Run(std::move(response_body));
      return;
    }

    LOG(ERROR) << "Downloading to string failed with error code: "
               << GetResponseCode(simple_loader.get()) << " with network error"
               << simple_loader->NetError();
    std::move(callback).Run(std::make_unique<std::string>());
  }

  void OnUrlDownloadedToFile(
      network::SimpleURLLoader::DownloadToFileCompleteCallback callback,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      const base::FilePath& desired_path,
      base::FilePath temp_path) {
    if (simple_loader->NetError() != net::OK || temp_path.empty()) {
      LOG(ERROR) << "Downloading to file failed with error code: "
                 << GetResponseCode(simple_loader.get())
                 << " with network error" << simple_loader->NetError();

      if (!temp_path.empty()) {
        // Clean up temporary file.
        task_runner_->PostTask(FROM_HERE, base::BindOnce(
                                              [](const base::FilePath& path) {
                                                base::DeleteFile(path);
                                              },
                                              temp_path));
      }
      std::move(callback).Run(base::FilePath());
      return;
    }

    // Swap the temporary file to the desired path, and then run the callback.
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& to_path, const base::FilePath& from_path) {
              if (!base::ReplaceFile(from_path, to_path,
                                     /*error=*/nullptr)) {
                LOG(ERROR)
                    << "Unable to move downloaded file to ambient directory";
                // Clean up the files.
                base::DeleteFile(from_path);
                base::DeleteFile(to_path);
                return base::FilePath();
              }
              return to_path;
            },
            desired_path, temp_path),
        std::move(callback));
  }

  int GetResponseCode(network::SimpleURLLoader* simple_loader) {
    if (simple_loader->ResponseInfo() &&
        simple_loader->ResponseInfo()->headers) {
      return simple_loader->ResponseInfo()->headers->response_code();
    } else {
      return -1;
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<AmbientURLLoaderImpl> weak_factory_{this};
};

class AmbientImageDecoderImpl : public AmbientImageDecoder {
 public:
  AmbientImageDecoderImpl() = default;
  ~AmbientImageDecoderImpl() override = default;

  // AmbientImageDecoder:
  void Decode(
      const std::vector<uint8_t>& encoded_bytes,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) override {
    data_decoder::DecodeImageIsolated(
        encoded_bytes, data_decoder::mojom::ImageCodec::DEFAULT,
        /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
        /*desired_image_frame_size=*/gfx::Size(),
        base::BindOnce(&ToImageSkia, std::move(callback)));
  }
};

AmbientPhotoController::AmbientPhotoController()
    : fetch_topic_retry_backoff_(&kFetchTopicRetryBackoffPolicy),
      resume_fetch_image_backoff_(&kResumeFetchImageBackoffPolicy),
      url_loader_(std::make_unique<AmbientURLLoaderImpl>()),
      image_decoder_(std::make_unique<AmbientImageDecoderImpl>()),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(GetTaskTraits())) {
  ambient_backend_model_observer_.Add(&ambient_backend_model_);
}

AmbientPhotoController::~AmbientPhotoController() = default;

void AmbientPhotoController::StartScreenUpdate() {
  FetchTopics();
  FetchWeather();
  weather_refresh_timer_.Start(
      FROM_HERE, kWeatherRefreshInterval,
      base::BindRepeating(&AmbientPhotoController::FetchWeather,
                          weak_factory_.GetWeakPtr()));
  if (backup_photo_refresh_timer_.IsRunning()) {
    // Would use |timer_.FireNow()| but this does not execute if screen is
    // locked. Manually call the expected callback instead.
    backup_photo_refresh_timer_.Stop();
    PrepareFetchBackupImages();
  }
}

void AmbientPhotoController::StopScreenUpdate() {
  photo_refresh_timer_.Stop();
  weather_refresh_timer_.Stop();
  topic_index_ = 0;
  image_refresh_started_ = false;
  retries_to_read_from_cache_ = kMaxNumberOfCachedImages;
  backup_retries_to_read_from_cache_ = GetBackupPhotoUrls().size();
  fetch_topic_retry_backoff_.Reset();
  resume_fetch_image_backoff_.Reset();
  ambient_backend_model_.Clear();
  weak_factory_.InvalidateWeakPtrs();
}

void AmbientPhotoController::ScheduleFetchBackupImages() {
  if (backup_photo_refresh_timer_.IsRunning())
    return;

  backup_photo_refresh_timer_.Start(
      FROM_HERE,
      std::max(kBackupPhotoRefreshDelay,
               resume_fetch_image_backoff_.GetTimeUntilRelease()),
      base::BindOnce(&AmbientPhotoController::PrepareFetchBackupImages,
                     weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::OnTopicsChanged() {
  if (ambient_backend_model_.topics().size() < kMaxNumberOfCachedImages)
    ScheduleFetchTopics(/*backoff=*/false);

  if (!image_refresh_started_) {
    image_refresh_started_ = true;
    ScheduleRefreshImage();
  }
}

void AmbientPhotoController::FetchTopics() {
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchScreenUpdateInfo(
          kTopicsBatchSize,
          base::BindOnce(&AmbientPhotoController::OnScreenUpdateInfoFetched,
                         weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::FetchWeather() {
  Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->FetchWeather(base::BindOnce(
          &AmbientPhotoController::StartDownloadingWeatherConditionIcon,
          weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::ClearCache() {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(
                             [](const base::FilePath& file_path) {
                               base::DeletePathRecursively(file_path);
                             },
                             GetCachePath()));
}

void AmbientPhotoController::ScheduleFetchTopics(bool backoff) {
  // If retry, using the backoff delay, otherwise the default delay.
  const base::TimeDelta delay =
      backoff ? fetch_topic_retry_backoff_.GetTimeUntilRelease()
              : kTopicFetchInterval;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AmbientPhotoController::FetchTopics,
                     weak_factory_.GetWeakPtr()),
      delay);
}

void AmbientPhotoController::ScheduleRefreshImage() {
  photo_refresh_timer_.Start(
      FROM_HERE, ambient_backend_model_.GetPhotoRefreshInterval(),
      base::BindOnce(&AmbientPhotoController::FetchPhotoRawData,
                     weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::PrepareFetchBackupImages() {
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce([]() { CreateDirIfNotExists(GetBackupCachePath()); }),
      base::BindOnce(&AmbientPhotoController::FetchBackupImages,
                     weak_factory_.GetWeakPtr()));
}

void AmbientPhotoController::FetchBackupImages() {
  const auto& backup_photo_urls = GetBackupPhotoUrls();
  backup_retries_to_read_from_cache_ = backup_photo_urls.size();
  for (size_t i = 0; i < backup_photo_urls.size(); i++) {
    url_loader_->DownloadToFile(
        backup_photo_urls.at(i),
        base::BindOnce(&AmbientPhotoController::OnBackupImageFetched,
                       weak_factory_.GetWeakPtr()),
        GetBackupFilePath(i));
  }
}

void AmbientPhotoController::OnBackupImageFetched(base::FilePath file_path) {
  if (file_path.empty()) {
    // TODO(b/169807068) Change to retry individual failed images.
    resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/false);
    LOG(WARNING) << "Downloading backup image failed.";
    ScheduleFetchBackupImages();
    return;
  }
  resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/true);
}

const AmbientModeTopic* AmbientPhotoController::GetNextTopic() {
  const auto& topics = ambient_backend_model_.topics();
  // If no more topics, will read from cache.
  if (topic_index_ == topics.size())
    return nullptr;

  return &topics[topic_index_++];
}

void AmbientPhotoController::OnScreenUpdateInfoFetched(
    const ash::ScreenUpdate& screen_update) {
  // It is possible that |screen_update| is an empty instance if fatal errors
  // happened during the fetch.
  if (screen_update.next_topics.empty()) {
    LOG(WARNING) << "The screen update has no topics.";

    fetch_topic_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    ScheduleFetchTopics(/*backoff=*/true);
    if (!image_refresh_started_) {
      image_refresh_started_ = true;
      ScheduleRefreshImage();
    }
    return;
  }
  fetch_topic_retry_backoff_.InformOfRequest(/*succeeded=*/true);
  ambient_backend_model_.AppendTopics(screen_update.next_topics);
  StartDownloadingWeatherConditionIcon(screen_update.weather_info);
}

void AmbientPhotoController::ResetImageData() {
  image_data_.reset();
  related_image_data_.reset();
  image_details_.reset();

  image_ = gfx::ImageSkia();
  related_image_ = gfx::ImageSkia();
}

void AmbientPhotoController::FetchPhotoRawData() {
  const AmbientModeTopic* topic = GetNextTopic();
  ResetImageData();

  if (topic) {
    const int num_callbacks = (topic->related_image_url) ? 2 : 1;
    auto on_done = base::BarrierClosure(
        num_callbacks,
        base::BindOnce(&AmbientPhotoController::OnAllPhotoRawDataAvailable,
                       weak_factory_.GetWeakPtr(),
                       /*from_downloading=*/true));

    url_loader_->Download(
        topic->url,
        base::BindOnce(&AmbientPhotoController::OnPhotoRawDataAvailable,
                       weak_factory_.GetWeakPtr(),
                       /*from_downloading=*/true,
                       /*is_related_image=*/false, on_done,
                       std::make_unique<std::string>(topic->details)));

    if (topic->related_image_url) {
      url_loader_->Download(
          *(topic->related_image_url),
          base::BindOnce(&AmbientPhotoController::OnPhotoRawDataAvailable,
                         weak_factory_.GetWeakPtr(),
                         /*from_downloading=*/true,
                         /*is_related_image=*/true, on_done,
                         std::make_unique<std::string>(topic->details)));
    }
    return;
  }

  // If |topic| is nullptr, will try to read from disk cache.
  TryReadPhotoRawData();
}

void AmbientPhotoController::TryReadPhotoRawData() {
  ResetImageData();
  // Stop reading from cache after the max number of retries.
  if (retries_to_read_from_cache_ == 0) {
    if (backup_retries_to_read_from_cache_ == 0) {
      LOG(WARNING) << "Failed to read from cache";
      ambient_backend_model_.AddImageFailure();
      // Do not refresh image if image loading has failed repeatedly, or there
      // are no more topics to retry.
      if (ambient_backend_model_.ImageLoadingFailed() ||
          topic_index_ == ambient_backend_model_.topics().size()) {
        LOG(WARNING) << "Not attempting image refresh";
        image_refresh_started_ = false;
        return;
      }

      // Try to resume normal workflow with backoff.
      const base::TimeDelta delay =
          resume_fetch_image_backoff_.GetTimeUntilRelease();
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AmbientPhotoController::ScheduleRefreshImage,
                         weak_factory_.GetWeakPtr()),
          delay);
      return;
    }

    --backup_retries_to_read_from_cache_;
    // Try to read a backup image.
    auto photo_data = std::make_unique<std::string>();
    auto* photo_data_ptr = photo_data.get();
    auto on_done = base::BindRepeating(
        &AmbientPhotoController::OnAllPhotoRawDataAvailable,
        weak_factory_.GetWeakPtr(), /*from_downloading=*/false);

    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            [](size_t index, std::string* data) {
              if (!base::ReadFileToString(GetBackupFilePath(index), data)) {
                LOG(ERROR) << "Unable to read from backup cache.";
                data->clear();
              }
            },
            backup_cache_index_for_display_, photo_data_ptr),
        base::BindOnce(&AmbientPhotoController::OnPhotoRawDataAvailable,
                       weak_factory_.GetWeakPtr(), /*from_downloading=*/false,
                       /*is_related_image=*/false, std::move(on_done),
                       /*details=*/std::make_unique<std::string>(),
                       std::move(photo_data)));

    backup_cache_index_for_display_++;
    if (backup_cache_index_for_display_ == GetBackupPhotoUrls().size())
      backup_cache_index_for_display_ = 0;
    return;
  }

  --retries_to_read_from_cache_;
  std::string file_name = base::NumberToString(cache_index_for_display_);

  ++cache_index_for_display_;
  if (cache_index_for_display_ == kMaxNumberOfCachedImages)
    cache_index_for_display_ = 0;

  auto photo_data = std::make_unique<std::string>();
  auto photo_details = std::make_unique<std::string>();

  auto on_done =
      base::BindRepeating(&AmbientPhotoController::OnAllPhotoRawDataAvailable,
                          weak_factory_.GetWeakPtr(),
                          /*from_downloading=*/false);

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](const std::string& file_name, std::string* photo_data,
             std::string* photo_details) {
            if (!base::ReadFileToString(
                    GetCachePath().Append(file_name + kPhotoFileExt),
                    photo_data)) {
              photo_data->clear();
            }
            if (!base::ReadFileToString(
                    GetCachePath().Append(file_name + kPhotoDetailsFileExt),
                    photo_details)) {
              photo_details->clear();
            }
          },
          file_name, photo_data.get(), photo_details.get()),
      base::BindOnce(&AmbientPhotoController::OnPhotoRawDataAvailable,
                     weak_factory_.GetWeakPtr(), /*from_downloading=*/false,
                     /*is_related_image=*/false, std::move(on_done),
                     std::move(photo_details), std::move(photo_data)));
}

void AmbientPhotoController::OnPhotoRawDataAvailable(
    bool from_downloading,
    bool is_related_image,
    base::RepeatingClosure on_done,
    std::unique_ptr<std::string> details,
    std::unique_ptr<std::string> data) {
  if (is_related_image) {
    related_image_data_ = std::move(data);
  } else {
    image_data_ = std::move(data);
    image_details_ = std::move(details);
  }
  std::move(on_done).Run();
}

void AmbientPhotoController::OnAllPhotoRawDataAvailable(bool from_downloading) {
  if (!image_data_ || image_data_->empty()) {
    if (from_downloading) {
      LOG(ERROR) << "Failed to download image";
      resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/false);
    } else {
      LOG(WARNING) << "Failed to read image";
    }

    // Try to read from cache when failure happens.
    TryReadPhotoRawData();
    return;
  }

  const std::string file_name = base::NumberToString(cache_index_for_store_);
  // If the data is fetched from downloading, write to disk.
  // Note: WriteFile() could fail. The saved file name may not be continuous.
  if (from_downloading)
    ++cache_index_for_store_;
  if (cache_index_for_store_ == kMaxNumberOfCachedImages)
    cache_index_for_store_ = 0;

  const int num_callbacks = related_image_data_ ? 2 : 1;
  auto on_done = base::BarrierClosure(
      num_callbacks,
      base::BindOnce(&AmbientPhotoController::OnAllPhotoDecoded,
                     weak_factory_.GetWeakPtr(), from_downloading,
                     /*hash=*/base::SHA1HashString(*image_data_)));

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](const std::string& file_name, bool need_to_save,
             const std::string& data, const std::string& details) {
            if (need_to_save) {
              WriteFile(GetCachePath().Append(file_name + kPhotoFileExt), data);
              WriteFile(GetCachePath().Append(file_name + kPhotoDetailsFileExt),
                        details);
            }
          },
          file_name, from_downloading, *image_data_, *image_details_),
      base::BindOnce(&AmbientPhotoController::DecodePhotoRawData,
                     weak_factory_.GetWeakPtr(), from_downloading,
                     /*is_related_image=*/false, on_done,
                     std::move(image_data_)));

  if (related_image_data_) {
    DecodePhotoRawData(from_downloading, /*is_related_image=*/true, on_done,
                       std::move(related_image_data_));
  }
}

void AmbientPhotoController::DecodePhotoRawData(
    bool from_downloading,
    bool is_related_image,
    base::RepeatingClosure on_done,
    std::unique_ptr<std::string> data) {
  std::vector<uint8_t> image_bytes(data->begin(), data->end());
  image_decoder_->Decode(
      image_bytes, base::BindOnce(&AmbientPhotoController::OnPhotoDecoded,
                                  weak_factory_.GetWeakPtr(), from_downloading,
                                  is_related_image, std::move(on_done)));
}

void AmbientPhotoController::OnPhotoDecoded(bool from_downloading,
                                            bool is_related_image,
                                            base::RepeatingClosure on_done,
                                            const gfx::ImageSkia& image) {
  if (is_related_image)
    related_image_ = image;
  else
    image_ = image;

  std::move(on_done).Run();
}

void AmbientPhotoController::OnAllPhotoDecoded(bool from_downloading,
                                               const std::string& hash) {
  if (image_.isNull()) {
    LOG(WARNING) << "Image is null";
    if (from_downloading)
      resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/false);

    // Try to read from cache when failure happens.
    TryReadPhotoRawData();
    return;
  } else if (ambient_backend_model_.HashMatchesNextImage(hash)) {
    LOG(WARNING) << "Skipping loading duplicate image.";
    TryReadPhotoRawData();
    return;
  }

  retries_to_read_from_cache_ = kMaxNumberOfCachedImages;
  backup_retries_to_read_from_cache_ = GetBackupPhotoUrls().size();

  if (from_downloading)
    resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/true);

  PhotoWithDetails detailed_photo;
  detailed_photo.photo = image_;
  detailed_photo.related_photo = related_image_;
  detailed_photo.details = *image_details_;
  detailed_photo.hash = hash;

  ResetImageData();

  ambient_backend_model_.AddNextImage(std::move(detailed_photo));

  ScheduleRefreshImage();
}

void AmbientPhotoController::StartDownloadingWeatherConditionIcon(
    const base::Optional<WeatherInfo>& weather_info) {
  if (!weather_info) {
    LOG(WARNING) << "No weather info included in the response.";
    return;
  }

  if (!weather_info->temp_f.has_value()) {
    LOG(WARNING) << "No temperature included in weather info.";
    return;
  }

  if (weather_info->condition_icon_url.value_or(std::string()).empty()) {
    LOG(WARNING) << "No value found for condition icon url in the weather info "
                    "response.";
    return;
  }

  // Ideally we should avoid downloading from the same url again to reduce the
  // overhead, as it's unlikely that the weather condition is changing
  // frequently during the day.
  // TODO(meilinw): avoid repeated downloading by caching the last N url hashes,
  // where N should depend on the icon image size.
  DownloadImageFromUrl(
      weather_info->condition_icon_url.value(),
      base::BindOnce(&AmbientPhotoController::OnWeatherConditionIconDownloaded,
                     weak_factory_.GetWeakPtr(), weather_info->temp_f.value(),
                     weather_info->show_celsius));
}

void AmbientPhotoController::OnWeatherConditionIconDownloaded(
    float temp_f,
    bool show_celsius,
    const gfx::ImageSkia& icon) {
  // For now we only show the weather card when both fields have values.
  // TODO(meilinw): optimize the behavior with more specific error handling.
  if (icon.isNull())
    return;

  ambient_backend_model_.UpdateWeatherInfo(icon, temp_f, show_celsius);
}

void AmbientPhotoController::FetchTopicsForTesting() {
  FetchTopics();
}

void AmbientPhotoController::FetchImageForTesting() {
  FetchPhotoRawData();
}

void AmbientPhotoController::FetchBackupImagesForTesting() {
  PrepareFetchBackupImages();
}

}  // namespace ash
