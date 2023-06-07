// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/managed/screensaver_images_policy_handler.h"

#include <memory>

#include "ash/constants/ash_paths.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

constexpr char kCacheDirectoryName[] = "managed_screensaver";
constexpr char kManagedGuestsCacheDirectoryPath[] = "guest";
constexpr char kSigninCacheDirectoryPath[] = "signin";

base::FilePath GetPolicyHandlerCachePath(
    ScreensaverImagesPolicyHandler::HandlerType state) {
  switch (state) {
    case ScreensaverImagesPolicyHandler::HandlerType::kSignin:
      return base::PathService::CheckedGet(
                 ash::DIR_DEVICE_POLICY_SCREENSAVER_DATA)
          .AppendASCII(kSigninCacheDirectoryPath);
    case ScreensaverImagesPolicyHandler::HandlerType::kManagedGuest:
      return base::PathService::CheckedGet(
                 ash::DIR_DEVICE_POLICY_SCREENSAVER_DATA)
          .AppendASCII(kManagedGuestsCacheDirectoryPath);
    case ScreensaverImagesPolicyHandler::HandlerType::kUser:
      return base::PathService::CheckedGet(base::DIR_HOME)
          .AppendASCII(kCacheDirectoryName);
  }
}

ScreensaverImagesPolicyHandler::HandlerType GetHandlerState(
    PrefService* pref_service) {
  auto* session_controller = Shell::Get()->session_controller();
  if (pref_service == session_controller->GetPrimaryUserPrefService() &&
      session_controller->IsUserPublicAccount()) {
    return ScreensaverImagesPolicyHandler::HandlerType::kManagedGuest;
  }
  if (pref_service == session_controller->GetPrimaryUserPrefService()) {
    return ScreensaverImagesPolicyHandler::HandlerType::kUser;
  }
  if (pref_service == session_controller->GetSigninScreenPrefService()) {
    return ScreensaverImagesPolicyHandler::HandlerType::kSignin;
  }
  LOG(DFATAL) << "Invalid pref store detected";
  return ScreensaverImagesPolicyHandler::HandlerType::kSignin;
}

scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory(
    ScreensaverImagesPolicyHandler::HandlerType state) {
  AmbientClient& ambient_client = CHECK_DEREF(AmbientClient::Get());
  switch (state) {
    case ScreensaverImagesPolicyHandler::HandlerType::kSignin:
      return ambient_client.GetSigninURLLoaderFactory();
    case ScreensaverImagesPolicyHandler::HandlerType::kManagedGuest:
    case ScreensaverImagesPolicyHandler::HandlerType::kUser:
      return ambient_client.GetURLLoaderFactory();
  }
}

}  // namespace

// static
std::unique_ptr<ScreensaverImagesPolicyHandler>
ScreensaverImagesPolicyHandler::Create(PrefService* pref_service) {
  // TODO(b/282134276): Move to a separate factory class to move creation
  // complexity out of this class and  isolate it,
  HandlerType state = GetHandlerState(pref_service);
  return std::make_unique<ScreensaverImagesPolicyHandler>(pref_service, state);
}

// static
void ScreensaverImagesPolicyHandler::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(
      ambient::prefs::kAmbientModeManagedScreensaverImages);
}

ScreensaverImagesPolicyHandler::ScreensaverImagesPolicyHandler(
    PrefService* pref_service,
    HandlerType state) {
  CHECK(pref_service);
  pref_service_ = pref_service;
  auto url_loader_factory = GetUrlLoaderFactory(state);
  base::FilePath cache_path = GetPolicyHandlerCachePath(state);
  image_downloader_ = std::make_unique<ScreensaverImageDownloader>(
      url_loader_factory, cache_path,
      base::BindRepeating(
          &ScreensaverImagesPolicyHandler::OnDownloadedImageListUpdated,
          base::Unretained(this)));

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  // TODO(b/286011972): Consider observing kAmbientModeManagedScreensaverEnabled
  // pref to clean up the cache when the policy is set to false.

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
  const PrefService::Preference& images_pref =
      CHECK_DEREF(pref_service_->FindPreference(
          ash::ambient::prefs::kAmbientModeManagedScreensaverImages));

  // Ignore default value (not set by policy) to prevent cache invalidation.
  if (images_pref.IsDefaultValue() && !IsManagedScreensaverDisabledByPolicy()) {
    return;
  }

  image_downloader_->UpdateImageUrlList(images_pref.GetValue()->GetList());
}

bool ScreensaverImagesPolicyHandler::IsManagedScreensaverDisabledByPolicy() {
  const PrefService::Preference& enabled_pref =
      CHECK_DEREF(pref_service_->FindPreference(
          ash::ambient::prefs::kAmbientModeManagedScreensaverEnabled));
  return !enabled_pref.IsDefaultValue() && !enabled_pref.GetValue()->GetBool();
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
  // Also fire the callback so that test expectations are met.
  if (on_images_updated_callback_) {
    on_images_updated_callback_.Run(images_file_paths);
  }
}

std::vector<base::FilePath>
ScreensaverImagesPolicyHandler::GetScreensaverImages() {
  if (image_downloader_) {
    return image_downloader_->GetScreensaverImages();
  }
  return std::vector<base::FilePath>();
}

}  // namespace ash
