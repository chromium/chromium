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
#include "ash/ambient/ambient_photo_cache.h"
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
#include "net/traffic_annotation/network_traffic_annotation.h"
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

void DownloadImageFromUrl(
    const std::string& url,
    base::OnceCallback<void(const gfx::ImageSkia&)> callback) {
  DCHECK(!url.empty());

  // During shutdown, we may not have `ImageDownloader` when reach here.
  if (!ImageDownloader::Get())
    return;

  ImageDownloader::Get()->Download(GURL(url), NO_TRAFFIC_ANNOTATION_YET,
                                   base::BindOnce(std::move(callback)));
}

base::TaskTraits GetTaskTraits() {
  return {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
}

const std::array<const char*, 2>& GetBackupPhotoUrls() {
  return Shell::Get()
      ->ambient_controller()
      ->ambient_backend_controller()
      ->GetBackupPhotoUrls();
}

// Get the cache root path for ambient mode.
base::FilePath GetCacheRootPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));
}

}  // namespace

AmbientPhotoController::AmbientPhotoController()
    : fetch_topic_retry_backoff_(&kFetchTopicRetryBackoffPolicy),
      resume_fetch_image_backoff_(&kResumeFetchImageBackoffPolicy),
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
    FetchBackupImages();
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
      base::BindOnce(&AmbientPhotoController::FetchBackupImages,
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
  DCHECK(photo_cache_);
  DCHECK(backup_photo_cache_);
  photo_cache_->Clear();
  backup_photo_cache_->Clear();
}

void AmbientPhotoController::InitCache() {
  if (!photo_cache_) {
    photo_cache_ = AmbientPhotoCache::Create(GetCacheRootPath().Append(
        FILE_PATH_LITERAL(kAmbientModeCacheDirectoryName)));
  }
  if (!backup_photo_cache_) {
    backup_photo_cache_ = AmbientPhotoCache::Create(GetCacheRootPath().Append(
        FILE_PATH_LITERAL(kAmbientModeBackupCacheDirectoryName)));
  }
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

void AmbientPhotoController::FetchBackupImages() {
  const auto& backup_photo_urls = GetBackupPhotoUrls();
  backup_retries_to_read_from_cache_ = backup_photo_urls.size();
  for (size_t i = 0; i < backup_photo_urls.size(); i++) {
    backup_photo_cache_->DownloadPhotoToFile(
        backup_photo_urls.at(i),
        /*cache_index=*/i,
        /*is_related=*/false,
        base::BindOnce(&AmbientPhotoController::OnBackupImageFetched,
                       weak_factory_.GetWeakPtr()));
  }
}

void AmbientPhotoController::OnBackupImageFetched(bool success) {
  if (!success) {
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
  cache_entry_.reset();

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
        base::BindOnce(&AmbientPhotoController::OnAllPhotoRawDataDownloaded,
                       weak_factory_.GetWeakPtr()));

    photo_cache_->DownloadPhoto(
        topic->url,
        base::BindOnce(&AmbientPhotoController::OnPhotoRawDataDownloaded,
                       weak_factory_.GetWeakPtr(),
                       /*is_related_image=*/false, on_done,
                       std::make_unique<std::string>(topic->details)));

    if (topic->related_image_url) {
      photo_cache_->DownloadPhoto(
          *(topic->related_image_url),
          base::BindOnce(&AmbientPhotoController::OnPhotoRawDataDownloaded,
                         weak_factory_.GetWeakPtr(),
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

    DVLOG(3) << "Read from backup cache index: "
             << backup_cache_index_for_display_;
    // Try to read a backup image.
    backup_photo_cache_->ReadFiles(
        /*cache_index=*/backup_cache_index_for_display_,
        base::BindOnce(&AmbientPhotoController::OnAllPhotoRawDataAvailable,
                       weak_factory_.GetWeakPtr(), /*from_downloading=*/false));

    backup_cache_index_for_display_++;
    if (backup_cache_index_for_display_ == GetBackupPhotoUrls().size())
      backup_cache_index_for_display_ = 0;
    return;
  }

  --retries_to_read_from_cache_;
  int current_cache_index = cache_index_for_display_;

  ++cache_index_for_display_;
  if (cache_index_for_display_ == kMaxNumberOfCachedImages)
    cache_index_for_display_ = 0;

  DVLOG(3) << "Read from cache index: " << current_cache_index;
  photo_cache_->ReadFiles(
      current_cache_index,
      base::BindOnce(&AmbientPhotoController::OnAllPhotoRawDataAvailable,
                     weak_factory_.GetWeakPtr(), /*from_downloading=*/false));
}

void AmbientPhotoController::OnPhotoRawDataDownloaded(
    bool is_related_image,
    base::RepeatingClosure on_done,
    std::unique_ptr<std::string> details,
    std::unique_ptr<std::string> data) {
  cache_entry_.details = std::move(details);

  if (is_related_image)
    cache_entry_.related_image = std::move(data);
  else
    cache_entry_.image = std::move(data);

  std::move(on_done).Run();
}

void AmbientPhotoController::OnAllPhotoRawDataDownloaded() {
  OnAllPhotoRawDataAvailable(/*from_downloading=*/true,
                             std::move(cache_entry_));
}

void AmbientPhotoController::OnAllPhotoRawDataAvailable(
    bool from_downloading,
    PhotoCacheEntry cache_entry) {
  if (!cache_entry.image || cache_entry.image->empty()) {
    if (from_downloading) {
      LOG(ERROR) << "Failed to download image";
      resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/false);
    }
    // Try to read from cache when failure happens.
    TryReadPhotoRawData();
    return;
  }

  if (from_downloading) {
    // If the data is fetched from downloading, write to disk.
    // Note: WriteFiles could fail. The saved file name may not be continuous.
    DVLOG(3) << "Save photo to cache index: " << cache_index_for_store_;
    auto current_cache_index = cache_index_for_store_;
    ++cache_index_for_store_;
    if (cache_index_for_store_ == kMaxNumberOfCachedImages)
      cache_index_for_store_ = 0;

    auto* image = cache_entry.image.get();
    auto* details = cache_entry.details.get();
    auto* related_image = cache_entry.related_image.get();

    photo_cache_->WriteFiles(
        /*cache_index=*/current_cache_index, image, details, related_image,
        base::BindOnce(&AmbientPhotoController::OnPhotoRawDataSaved,
                       weak_factory_.GetWeakPtr(), from_downloading,
                       std::move(cache_entry)));
  } else {
    OnPhotoRawDataSaved(from_downloading, std::move(cache_entry));
  }
}

void AmbientPhotoController::OnPhotoRawDataSaved(bool from_downloading,
                                                 PhotoCacheEntry cache_entry) {
  bool has_related =
      cache_entry.related_image && !cache_entry.related_image->empty();
  const int num_callbacks = has_related ? 2 : 1;

  auto on_done = base::BarrierClosure(
      num_callbacks,
      base::BindOnce(&AmbientPhotoController::OnAllPhotoDecoded,
                     weak_factory_.GetWeakPtr(), from_downloading,
                     /*hash=*/base::SHA1HashString(*cache_entry.image)));

  DecodePhotoRawData(from_downloading,
                     /*is_related_image=*/false, on_done,
                     std::move(cache_entry.image));

  if (has_related) {
    DecodePhotoRawData(from_downloading, /*is_related_image=*/true, on_done,
                       std::move(cache_entry.related_image));
  }
}

void AmbientPhotoController::DecodePhotoRawData(
    bool from_downloading,
    bool is_related_image,
    base::RepeatingClosure on_done,
    std::unique_ptr<std::string> data) {
  photo_cache_->DecodePhoto(
      std::move(data),
      base::BindOnce(&AmbientPhotoController::OnPhotoDecoded,
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
    LOG(WARNING) << "Image decoding failed";
    if (from_downloading)
      resume_fetch_image_backoff_.InformOfRequest(/*succeeded=*/false);

    // Try to read from cache when failure happens.
    TryReadPhotoRawData();
    return;
  } else if (ambient_backend_model_.IsHashDuplicate(hash)) {
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
  if (cache_entry_.details)
    detailed_photo.details = *cache_entry_.details;
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
  FetchBackupImages();
}

}  // namespace ash
