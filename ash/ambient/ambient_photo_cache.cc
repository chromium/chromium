// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_cache.h"

#include <fstream>
#include <iostream>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_photo_cache_settings.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

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

const base::FilePath& GetCacheRootDir(ambient_photo_cache::Store store) {
  switch (store) {
    case ambient_photo_cache::Store::kPrimary:
      return GetAmbientPhotoCacheRootDir();
    case ambient_photo_cache::Store::kBackup:
      return GetAmbientBackupPhotoCacheRootDir();
  }
  NOTREACHED() << "Unknown cache store: " << static_cast<int>(store);
}

base::FilePath GetCachePath(int cache_index, const base::FilePath& root_path) {
  return root_path.Append(base::NumberToString(cache_index) + kPhotoCacheExt);
}

scoped_refptr<base::SequencedTaskRunner>& GetFileTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      kFileTaskRunner;
  return *kFileTaskRunner;
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

void OnUrlDownloadedToTempFile(
    base::OnceCallback<void(base::FilePath)> callback,
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    base::FilePath temp_path) {
  CHECK(callback);
  if (simple_loader->NetError() != net::OK || temp_path.empty()) {
    LOG(ERROR) << "Downloading to file failed with error code: "
               << GetResponseCode(simple_loader.get()) << " with network error "
               << simple_loader->NetError();

    if (!temp_path.empty()) {
      // Clean up temporary file.
      GetFileTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](const base::FilePath& path) { base::DeleteFile(path); },
              temp_path));
    }
    std::move(callback).Run(base::FilePath());
    return;
  }
  std::move(callback).Run(std::move(temp_path));
}

void DownloadPhotoInternal(
    const std::string& url,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    base::OnceCallback<void(std::string&&)> callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      CreateSimpleURLLoader(url, access_token);
  auto* loader_ptr = simple_loader.get();
  auto* loader_factory_ptr = loader_factory.get();

  loader_ptr->DownloadToString(
      loader_factory_ptr,
      base::BindOnce(&OnUrlDownloaded, std::move(callback),
                     std::move(simple_loader), std::move(loader_factory)),
      kMaxImageSizeInBytes);
}

void DownloadPhotoToTempFileInternal(
    const std::string& url,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    base::OnceCallback<void(base::FilePath)> callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      CreateSimpleURLLoader(url, access_token);
  auto* loader_ptr = simple_loader.get();
  auto* loader_factory_ptr = loader_factory.get();
  loader_ptr->DownloadToTempFile(
      loader_factory_ptr,
      base::BindOnce(&OnUrlDownloadedToTempFile, std::move(callback),
                     std::move(simple_loader), std::move(loader_factory)));
}

}  // namespace

namespace ambient_photo_cache {

void SetFileTaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner) {
  GetFileTaskRunner() = std::move(task_runner);
}

void DownloadPhoto(const std::string& url,
                   AmbientAccessTokenController& access_token_controller,
                   base::OnceCallback<void(std::string&&)> callback) {
  access_token_controller.RequestAccessToken(base::BindOnce(
      &DownloadPhotoInternal, url, AmbientClient::Get()->GetURLLoaderFactory(),
      std::move(callback)));
}

void DownloadPhotoToTempFile(
    const std::string& url,
    AmbientAccessTokenController& access_token_controller,
    base::OnceCallback<void(base::FilePath)> callback) {
  access_token_controller.RequestAccessToken(base::BindOnce(
      &DownloadPhotoToTempFileInternal, url,
      AmbientClient::Get()->GetURLLoaderFactory(), std::move(callback)));
}

void WritePhotoCache(Store store,
                     int cache_index,
                     const ambient::PhotoCacheEntry& cache_entry,
                     base::OnceClosure callback) {
  DCHECK_LT(cache_index, kMaxNumberOfCachedImages);
  GetFileTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](int cache_index, const base::FilePath& root_path,
             const ambient::PhotoCacheEntry& cache_entry) {
            auto cache_path = GetCachePath(cache_index, root_path);
            WriteOrDeleteFile(cache_path, cache_entry);
          },
          cache_index, GetCacheRootDir(store), cache_entry),
      std::move(callback));
}

void ReadPhotoCache(
    Store store,
    int cache_index,
    base::OnceCallback<void(::ambient::PhotoCacheEntry)> callback) {
  GetFileTaskRunner()->PostTaskAndReplyWithResult(
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
          cache_index, GetCacheRootDir(store)),
      std::move(callback));
}

void Clear(Store store) {
  GetFileTaskRunner()->PostTask(FROM_HERE,
                                base::BindOnce(
                                    [](const base::FilePath& file_path) {
                                      base::DeletePathRecursively(file_path);
                                    },
                                    GetCacheRootDir(store)));
}

}  // namespace ambient_photo_cache
}  // namespace ash
