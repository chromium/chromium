// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_cache.h"

#include <fstream>
#include <iostream>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

constexpr net::NetworkTrafficAnnotationTag kAmbientPhotoCacheNetworkTag =
    net::DefineNetworkTrafficAnnotation("ambient_photo_cache", R"(
        semantics {
          sender: "Ambient photo"
          description:
            "Get ambient photo from url to store limited number of photos in "
            "the device cache. This is used to show the screensaver when the "
            "user is idle. The url can be Backdrop service to provide pictures"
            " from internal gallery, weather/time photos served by Google, or "
            "user selected album from Google photos."
          trigger:
            "Triggered by a photo refresh timer, after the device has been "
            "idle and the battery is charging."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
         cookies_allowed: NO
         setting:
           "This feature is off by default and can be overridden by users."
         policy_exception_justification:
           "This feature is set by user settings.ambient_mode.enabled pref. "
           "The user setting is per device and cannot be overriden by admin."
        })");

void ToImageSkia(base::OnceCallback<void(const gfx::ImageSkia&)> callback,
                 const SkBitmap& image) {
  if (image.isNull()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(image);
  image_skia.MakeThreadSafe();

  std::move(callback).Run(image_skia);
}

// Helper function to extract response code from |SimpleURLLoader|.
int GetResponseCode(network::SimpleURLLoader* simple_loader) {
  if (simple_loader->ResponseInfo() && simple_loader->ResponseInfo()->headers)
    return simple_loader->ResponseInfo()->headers->response_code();
  else
    return -1;
}

std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(
    const std::string& url,
    const std::string& token) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  if (token.empty())
    DVLOG(2) << "Failed to fetch access token";
  else
    resource_request->headers.SetHeader("Authorization", "Bearer " + token);

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kAmbientPhotoCacheNetworkTag);
}

bool CreateDirIfNotExists(const base::FilePath& path) {
  return base::DirectoryExists(path) || base::CreateDirectory(path);
}

bool WriteOrDeleteFile(const base::FilePath& path,
                       const ambient::PhotoCacheEntry& cache_entry) {
  // If the primary photo is empty, the same as the related photo.
  if (!cache_entry.has_primary_photo() ||
      cache_entry.primary_photo().image().empty()) {
    base::DeleteFile(path);
    return false;
  }

  if (!CreateDirIfNotExists(path.DirName())) {
    LOG(ERROR) << "Cannot create ambient mode directory.";
    return false;
  }

  if (base::SysInfo::AmountOfFreeDiskSpace(path.DirName()) <
      kMaxReservedAvailableDiskSpaceByte) {
    LOG(ERROR) << "Not enough disk space left.";
    return false;
  }

  // Create a temp file.
  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(path.DirName(), &temp_file)) {
    LOG(ERROR) << "Cannot create a temporary file.";
    return false;
  }

  // Write to the tmp file.
  const char* path_str = temp_file.value().c_str();
  std::fstream output(path_str,
                      std::ios::out | std::ios::trunc | std::ios::binary);
  if (!cache_entry.SerializeToOstream(&output)) {
    LOG(ERROR) << "Cannot write the temporary file.";
    base::DeleteFile(temp_file);
    return false;
  }

  // Replace the current file with the temp file.
  if (!base::ReplaceFile(temp_file, path, /*error=*/nullptr)) {
    LOG(ERROR) << "Cannot replace the temporary file.";
    base::DeleteFile(temp_file);
    base::DeleteFile(path);
    return false;
  }

  return true;
}

base::FilePath GetCachePath(int cache_index, const base::FilePath& root_path) {
  return root_path.Append(base::NumberToString(cache_index) + kPhotoCacheExt);
}

base::RepeatingCallback<std::unique_ptr<AmbientPhotoCache>()>&
GetCustomFactoryFunction() {
  static base::NoDestructor<
      base::RepeatingCallback<std::unique_ptr<AmbientPhotoCache>()>>
      g_custom_factory;
  return *g_custom_factory;
}

// -----------------AmbientPhotoCacheImpl---------------------------------------

class AmbientPhotoCacheImpl : public AmbientPhotoCache {
 public:
  AmbientPhotoCacheImpl(base::FilePath path,
                        AmbientClient& ambient_client,
                        AmbientAccessTokenController& access_token_controller)
      : root_directory_(path),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
        ambient_client_(ambient_client),
        access_token_controller_(access_token_controller) {}

  ~AmbientPhotoCacheImpl() override = default;

  // AmbientPhotoCache:
  void DownloadPhoto(
      const std::string& url,
      base::OnceCallback<void(std::string&&)> callback) override {
    access_token_controller_->RequestAccessToken(
        base::BindOnce(&AmbientPhotoCacheImpl::DownloadPhotoInternal,
                       weak_factory_.GetWeakPtr(), url, std::move(callback)));
  }

  void DownloadPhotoToFile(const std::string& url,
                           int cache_index,
                           base::OnceCallback<void(bool)> callback) override {
    auto file_path = GetCachePath(cache_index, root_directory_);
    base::OnceClosure download_callback;
    download_callback = base::BindOnce(
        [](base::WeakPtr<AmbientPhotoCacheImpl> weak_ptr,
           base::OnceCallback<void(const std::string&, const std::string&)>
               callback) {
          if (!weak_ptr)
            return;
          weak_ptr->access_token_controller_->RequestAccessToken(
              std::move(callback));
        },
        weak_factory_.GetWeakPtr(),
        base::BindOnce(&AmbientPhotoCacheImpl::DownloadPhotoToFileInternal,
                       weak_factory_.GetWeakPtr(), url, std::move(callback),
                       file_path));

    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& path) {
              if (!CreateDirIfNotExists(path))
                LOG(ERROR) << "Cannot create ambient mode directory";
            },
            root_directory_),
        std::move(download_callback));
  }

  void DecodePhoto(
      const std::string& data,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) override {
    data_decoder::DecodeImageIsolated(
        base::as_bytes(base::make_span(data)),
        data_decoder::mojom::ImageCodec::kDefault,
        /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
        /*desired_image_frame_size=*/gfx::Size(),
        base::BindOnce(&ToImageSkia, std::move(callback)));
  }

  void WritePhotoCache(int cache_index,
                       const ambient::PhotoCacheEntry& cache_entry,
                       base::OnceClosure callback) override {
    DCHECK_LT(cache_index, kMaxNumberOfCachedImages);
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            [](int cache_index, const base::FilePath& root_path,
               const ambient::PhotoCacheEntry& cache_entry) {
              auto cache_path = GetCachePath(cache_index, root_path);
              WriteOrDeleteFile(cache_path, cache_entry);
            },
            cache_index, root_directory_, cache_entry),
        std::move(callback));
  }

  void ReadPhotoCache(
      int cache_index,
      base::OnceCallback<void(::ambient::PhotoCacheEntry)> callback) override {
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](int cache_index, const base::FilePath& root_path) {
              auto cache_path = GetCachePath(cache_index, root_path);

              // Read the existing cache.
              const char* path_str = cache_path.value().c_str();
              std::fstream input(path_str, std::ios::in | std::ios::binary);
              ambient::PhotoCacheEntry cache_entry;
              if (!input || !cache_entry.ParseFromIstream(&input)) {
                LOG(ERROR) << "Unable to read photo cache";
                cache_entry = ::ambient::PhotoCacheEntry();
                base::DeleteFile(cache_path);
              }
              return cache_entry;
            },
            cache_index, root_directory_),
        std::move(callback));
  }

  void Clear() override {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(
                               [](const base::FilePath& file_path) {
                                 base::DeletePathRecursively(file_path);
                               },
                               root_directory_));
  }

 private:
  void DownloadPhotoInternal(const std::string& url,
                             base::OnceCallback<void(std::string&&)> callback,
                             const std::string& gaia_id,
                             const std::string& access_token) {
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        CreateSimpleURLLoader(url, access_token);
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
        ambient_client_->GetURLLoaderFactory();
    auto* loader_ptr = simple_loader.get();
    auto* loader_factory_ptr = loader_factory.get();

    loader_ptr->DownloadToString(
        loader_factory_ptr,
        base::BindOnce(&AmbientPhotoCacheImpl::OnUrlDownloaded,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(simple_loader), std::move(loader_factory)),
        kMaxImageSizeInBytes);
  }

  void DownloadPhotoToFileInternal(const std::string& url,
                                   base::OnceCallback<void(bool)> callback,
                                   const base::FilePath& file_path,
                                   const std::string& gaia_id,
                                   const std::string& access_token) {
    std::unique_ptr<network::SimpleURLLoader> simple_loader =
        CreateSimpleURLLoader(url, access_token);
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
        ambient_client_->GetURLLoaderFactory();
    auto* loader_ptr = simple_loader.get();
    auto* loader_factory_ptr = loader_factory.get();

    // Create a temporary file path as target for download to guard against race
    // conditions in reading.
    base::FilePath temp_path =
        file_path.DirName().Append(base::UnguessableToken::Create().ToString());

    // Download to temp file first to guarantee entire image is written without
    // errors before attempting to read it.
    loader_ptr->DownloadToFile(
        loader_factory_ptr,
        base::BindOnce(&AmbientPhotoCacheImpl::OnUrlDownloadedToFile,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(simple_loader), std::move(loader_factory),
                       file_path),
        temp_path);
  }

  void OnUrlDownloaded(
      base::OnceCallback<void(std::string&&)> callback,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      std::unique_ptr<std::string> response_body) {
    if (simple_loader->NetError() == net::OK && response_body) {
      std::move(callback).Run(std::move(*response_body));
      return;
    }

    LOG(ERROR) << "Downloading to string failed with error code: "
               << GetResponseCode(simple_loader.get()) << " with network error "
               << simple_loader->NetError();
    std::move(callback).Run(std::string());
  }

  void OnUrlDownloadedToFile(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      const base::FilePath& desired_path,
      base::FilePath temp_path) {
    if (simple_loader->NetError() != net::OK || temp_path.empty()) {
      LOG(ERROR) << "Downloading to file failed with error code: "
                 << GetResponseCode(simple_loader.get())
                 << " with network error " << simple_loader->NetError();

      if (!temp_path.empty()) {
        // Clean up temporary file.
        task_runner_->PostTask(FROM_HERE, base::BindOnce(
                                              [](const base::FilePath& path) {
                                                base::DeleteFile(path);
                                              },
                                              temp_path));
      }
      std::move(callback).Run(false);
      return;
    }

    // Swap the temporary file to the desired path, and then run the callback.
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& to_path, const base::FilePath& from_path) {
              ambient::PhotoCacheEntry cache_entry;
              ambient::Photo* primary_photo =
                  cache_entry.mutable_primary_photo();
              std::string image;
              bool has_error = false;

              if (!base::ReadFileToString(from_path, &image)) {
                has_error = true;
                LOG(ERROR) << "Unable to read downloaded file";
              } else {
                primary_photo->set_image(std::move(image));
              }

              if (!has_error && !WriteOrDeleteFile(to_path, cache_entry)) {
                has_error = true;
                LOG(ERROR)
                    << "Unable to move downloaded file to ambient directory";
              }

              base::DeleteFile(from_path);
              return !has_error;
            },
            desired_path, temp_path),
        std::move(callback));
  }

  const base::FilePath root_directory_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const raw_ref<AmbientClient, ExperimentalAsh> ambient_client_;
  const raw_ref<AmbientAccessTokenController, ExperimentalAsh>
      access_token_controller_;
  base::WeakPtrFactory<AmbientPhotoCacheImpl> weak_factory_{this};
};

}  // namespace

// -------------- AmbientPhotoCache --------------------------------------------

// static
std::unique_ptr<AmbientPhotoCache> AmbientPhotoCache::Create(
    base::FilePath root_path,
    AmbientClient& ambient_client,
    AmbientAccessTokenController& access_token_controller) {
  return GetCustomFactoryFunction()
             ? GetCustomFactoryFunction().Run()
             : std::make_unique<AmbientPhotoCacheImpl>(
                   root_path, ambient_client, access_token_controller);
}

// static
void AmbientPhotoCache::SetFactoryForTesting(
    base::RepeatingCallback<std::unique_ptr<AmbientPhotoCache>()> create_cb) {
  GetCustomFactoryFunction() = std::move(create_cb);
}

}  // namespace ash
