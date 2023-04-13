// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screensaver_images_policy_handler.h"

#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/shell.h"
#include "base/base64url.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

constexpr char kCacheDirectoryName[] = "managed_screensaver";
constexpr char kCacheFileExt[] = ".cache";

// This limit is specified in the policy definition for the policies
// ScreensaverLockScreenImages and DeviceScreensaverLoginScreenImages.
constexpr size_t kMaxUrlsToProcessFromPolicy = 25u;

std::string GetHashedNameForUrl(const std::string& url) {
  std::string hashed_url;
  base::Base64UrlEncode(base::SHA1HashString(url),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &hashed_url);
  return hashed_url + kCacheFileExt;
}

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

  // TODO(b/277729103): If the pref value is empty, delete files in the download
  // directory, clear `downloaded_images_`, and do not trigger new download
  // jobs.

  // TODO(b/271093110): Implement clean up logic before sending new download
  // jobs.

  const base::Value::List& urls_list = user_pref_service_->GetList(
      ash::ambient::prefs::kAmbientModeManagedScreensaverImages);
  for (size_t i = 0; i < kMaxUrlsToProcessFromPolicy && i < urls_list.size();
       ++i) {
    const base::Value& url = urls_list[i];
    if (!url.is_string() || url.GetString().empty()) {
      continue;
    }
    auto job = std::make_unique<ScreensaverImageDownloader::Job>(
        url.GetString(), GetHashedNameForUrl(url.GetString()),
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

  on_images_updated_callback_.Run(std::vector<base::FilePath>(
      downloaded_images_.begin(), downloaded_images_.end()));
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
