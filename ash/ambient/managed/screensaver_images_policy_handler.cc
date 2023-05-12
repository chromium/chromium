// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/managed/screensaver_images_policy_handler.h"

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/session/session_controller_impl.h"
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
constexpr char kManagedGuestsCacheDirectoryPath[] =
    "/var/cache/managed_screensaver/guest";

base::FilePath GetDownloaderRootPath() {
  SessionControllerImpl& session =
      CHECK_DEREF(Shell::Get()->session_controller());
  if (session.IsUserPublicAccount()) {
    return base::FilePath(kManagedGuestsCacheDirectoryPath);
  }
  // TODO(b/271093537): Support the folder location for sign-in screensaver.

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

ScreensaverImagesPolicyHandler::ScreensaverImagesPolicyHandler(
    PrefService* pref_service) {
  CHECK(pref_service);
  if (pref_service !=
      Shell::Get()->session_controller()->GetPrimaryUserPrefService()) {
    // TODO(b/271093537): Support the policy handler for the sign-in screen
    return;
  }
  user_pref_service_ = pref_service;

  AmbientClient& ambient_client = CHECK_DEREF(AmbientClient::Get());
  image_downloader_ = std::make_unique<ScreensaverImageDownloader>(
      ambient_client.GetURLLoaderFactory(), GetDownloaderRootPath(),
      base::BindRepeating(
          &ScreensaverImagesPolicyHandler::OnDownloadedImageListUpdated,
          base::Unretained(this)));

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  // TODO(b/271093110): Use weak ptr factory for this callback.
  pref_change_registrar_->Add(
      ambient::prefs::kAmbientModeManagedScreensaverImages,
      base::BindRepeating(&ScreensaverImagesPolicyHandler::
                              OnAmbientModeManagedScreensaverImagesPrefChanged,
                          base::Unretained(this)));

  OnAmbientModeManagedScreensaverImagesPrefChanged();
}

ScreensaverImagesPolicyHandler::~ScreensaverImagesPolicyHandler() = default;

void ScreensaverImagesPolicyHandler::
    OnAmbientModeManagedScreensaverImagesPrefChanged() {
  if (!user_pref_service_) {
    return;
  }

  const base::Value::List& url_list = user_pref_service_->GetList(
      ash::ambient::prefs::kAmbientModeManagedScreensaverImages);
  image_downloader_->UpdateImageUrlList(url_list);
}

void ScreensaverImagesPolicyHandler::OnDownloadedImageListUpdated(
    const std::vector<base::FilePath>& images) {
  if (on_images_updated_callback_) {
    on_images_updated_callback_.Run(images);
  }
}

void ScreensaverImagesPolicyHandler::SetScreensaverImagesUpdatedCallback(
    ScreensaverImagesRepeatingCallback callback) {
  CHECK(callback);
  on_images_updated_callback_ = std::move(callback);
}

void ScreensaverImagesPolicyHandler::SetImagesForTesting(
    const std::vector<base::FilePath>& images_file_paths) {
  image_downloader_->SetImagesForTesting(images_file_paths);  // IN-TEST
}

std::vector<base::FilePath>
ScreensaverImagesPolicyHandler::GetScreensaverImages() {
  if (image_downloader_) {
    return image_downloader_->GetScreensaverImages();
  }
  return std::vector<base::FilePath>();
}

}  // namespace ash
