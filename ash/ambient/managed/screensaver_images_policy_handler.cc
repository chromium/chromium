// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/managed/screensaver_images_policy_handler.h"

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/shell.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

constexpr char kCacheDirectoryName[] = "managed_screensaver";

// This limit is specified in the policy definition for the policies
// ScreensaverLockScreenImages and DeviceScreensaverLoginScreenImages.
constexpr size_t kMaxUrlsToProcessFromPolicy = 25u;

base::FilePath GetDownloaderRootPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.Append(FILE_PATH_LITERAL(kCacheDirectoryName));
}

}  // namespace

// static
void ScreensaverImagesPolicyHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(
      ambient::prefs::kAmbientModeManagedScreensaverImages);
}

ScreensaverImagesPolicyHandler::ScreensaverImagesPolicyHandler() = default;

ScreensaverImagesPolicyHandler::~ScreensaverImagesPolicyHandler() = default;

void ScreensaverImagesPolicyHandler::
    OnAmbientModeManagedScreensaverImagesPrefChanged() {
  if (!user_pref_service_) {
    return;
  }

  // TODO(b/278857721): Do not download/cache images if the ScreensaverEnabled
  // pref is false.

  const base::Value::List& url_list = user_pref_service_->GetList(
      ash::ambient::prefs::kAmbientModeManagedScreensaverImages);
  if (url_list.empty()) {
    // If the screensaver is listening to updates, notify that the images are no
    // longer available before deleting them.
    if (on_images_updated_callback_) {
      on_images_updated_callback_.Run(std::vector<base::FilePath>());
    }

    image_downloader_->ClearRequestQueue();
    weak_ptr_factory_.InvalidateWeakPtrs();
    image_downloader_->DeleteDownloadedImages();
    return;
  }

  for (size_t i = 0; i < kMaxUrlsToProcessFromPolicy && i < url_list.size();
       ++i) {
    const base::Value& url = url_list[i];
    if (!url.is_string() || url.GetString().empty()) {
      continue;
    }
    auto job = std::make_unique<ScreensaverImageDownloader::Job>(
        url.GetString(),
        base::BindOnce(&ScreensaverImagesPolicyHandler::OnDownloadJobCompleted,
                       weak_ptr_factory_.GetWeakPtr()));

    image_downloader_->QueueDownloadJob(std::move(job));
  }
}

void ScreensaverImagesPolicyHandler::OnDownloadJobCompleted(
    ScreensaverImageDownloadResult result,
    absl::optional<base::FilePath> path) {
  if (result != ScreensaverImageDownloadResult::kSuccess) {
    return;
  }
  CHECK(path.has_value());
  downloaded_images_.insert(*path);

  if (on_images_updated_callback_) {
    on_images_updated_callback_.Run(std::vector<base::FilePath>(
        downloaded_images_.begin(), downloaded_images_.end()));
  }
}

void ScreensaverImagesPolicyHandler::SetScreensaverImagesUpdatedCallback(
    ScreensaverImagesRepeatingCallback callback) {
  CHECK(callback);
  on_images_updated_callback_ = std::move(callback);
}

std::vector<base::FilePath>
ScreensaverImagesPolicyHandler::GetScreensaverImages() {
  return std::vector<base::FilePath>(downloaded_images_.begin(),
                                     downloaded_images_.end());
}

void ScreensaverImagesPolicyHandler::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_service !=
      Shell::Get()->session_controller()->GetPrimaryUserPrefService()) {
    return;
  }

  if (user_pref_service_) {
    return;
  }

  AmbientClient& ambient_client = CHECK_DEREF(AmbientClient::Get());
  image_downloader_ = std::make_unique<ScreensaverImageDownloader>(
      ambient_client.GetURLLoaderFactory(), GetDownloaderRootPath());

  user_pref_service_ = pref_service;
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeManagedScreensaverImages,
      base::BindRepeating(&ScreensaverImagesPolicyHandler::
                              OnAmbientModeManagedScreensaverImagesPrefChanged,
                          base::Unretained(this)));

  OnAmbientModeManagedScreensaverImagesPrefChanged();
}

}  // namespace ash
