// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_images_policy_handler.h"

#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

constexpr char kCacheDirectoryName[] = "managed_screensaver";

}  // namespace

// static
void ScreensaverImagesPolicyHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(
      ash::ambient::prefs::kAmbientModeManagedScreensaverImages);
}

ScreensaverImagesPolicyHandler::ScreensaverImagesPolicyHandler() = default;

ScreensaverImagesPolicyHandler::~ScreensaverImagesPolicyHandler() = default;

void ScreensaverImagesPolicyHandler::
    OnAmbientModeManagedScreensaverImagesPrefChanged() {
  if (!user_pref_service_) {
    return;
  }

  // TODO(b/271093572): Read the value from the pref and try to download
}

// TODO(b/271093572): Call this function when images have been downloaded
void ScreensaverImagesPolicyHandler::OnScreensaverImagesDownloaded() {
  // TODO(b/271093572): Run with all downloaded file paths.
  if (on_images_updated_callback_) {
    on_images_updated_callback_.Run({});
  }
}

void ScreensaverImagesPolicyHandler::SetScreensaverImagesUpdatedCallback(
    ScreensaverImagesRepeatingCallback callback) {
  CHECK(callback);
  on_images_updated_callback_ = std::move(callback);
}

std::vector<base::FilePath>
ScreensaverImagesPolicyHandler::GetScreensaverImages() {
  // TODO(b/271093572): return the file paths to the images that have been
  // already downloaded.
  return {};
}

void ScreensaverImagesPolicyHandler::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_service !=
      ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService()) {
    return;
  }

  if (user_pref_service_) {
    return;
  }

  user_pref_service_ = pref_service;
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  CHECK(primary_user);
  Profile* user_profile =
      ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
  CHECK(user_profile);

  const base::FilePath cache_root_directory =
      user_profile->GetPath().AppendASCII(kCacheDirectoryName);
  image_downloader_ = std::make_unique<ScreensaverImageDownloader>(
      user_profile->GetURLLoaderFactory(), cache_root_directory);

  pref_change_registrar_->Add(
      ash::ambient::prefs::kAmbientModeManagedScreensaverImages,
      base::BindRepeating(&ScreensaverImagesPolicyHandler::
                              OnAmbientModeManagedScreensaverImagesPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  OnAmbientModeManagedScreensaverImagesPrefChanged();
}

}  // namespace policy
