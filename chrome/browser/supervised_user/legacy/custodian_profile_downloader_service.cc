// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/legacy/custodian_profile_downloader_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"

CustodianProfileDownloaderService::CustodianProfileDownloaderService(
    Profile* custodian_profile)
        : custodian_profile_(custodian_profile) {
}

CustodianProfileDownloaderService::~CustodianProfileDownloaderService() {}

void CustodianProfileDownloaderService::Shutdown() {
  profile_downloader_.reset();
}

void CustodianProfileDownloaderService::DownloadProfile(
    const DownloadProfileCallback& callback) {
  // The user must be logged in.
  if (!IdentityManagerFactory::GetForProfile(custodian_profile_)
           ->HasPrimaryAccount()) {
    return;
  }

  download_callback_ = callback;
  std::string current_email = custodian_profile_->GetProfileUserName();
  if (gaia::AreEmailsSame(last_downloaded_profile_email_, current_email)) {
    // Profile was previously downloaded successfully, use it as it is unlikely
    // that we will need to download it again.
    OnProfileDownloadSuccess(profile_downloader_.get());
    return;
  }
  // If another profile download is in progress, drop it. It's not worth
  // queueing them up, and more likely that the one that hasn't ended yet is
  // failing somehow than that the new one won't succeed.
  in_progress_profile_email_ = current_email;
  profile_downloader_.reset(new ProfileDownloader(this));
  profile_downloader_->Start();
}

bool CustodianProfileDownloaderService::NeedsProfilePicture() const {
  return false;
}

int CustodianProfileDownloaderService::GetDesiredImageSideLength() const {
  return 0;
}

std::string CustodianProfileDownloaderService::GetCachedPictureURL() const {
  return std::string();
}

signin::IdentityManager*
CustodianProfileDownloaderService::GetIdentityManager() {
  DCHECK(custodian_profile_);
  return IdentityManagerFactory::GetForProfile(custodian_profile_);
}

network::mojom::URLLoaderFactory*
CustodianProfileDownloaderService::GetURLLoaderFactory() {
  DCHECK(custodian_profile_);
  return content::BrowserContext::GetDefaultStoragePartition(custodian_profile_)
      ->GetURLLoaderFactoryForBrowserProcess()
      .get();
}

bool CustodianProfileDownloaderService::IsPreSignin() const {
  return false;
}

void CustodianProfileDownloaderService::OnProfileDownloadSuccess(
    ProfileDownloader* downloader) {
  download_callback_.Run(downloader->GetProfileFullName());
  download_callback_.Reset();
  last_downloaded_profile_email_ = in_progress_profile_email_;
}

void CustodianProfileDownloaderService::OnProfileDownloadFailure(
    ProfileDownloader* downloader,
    ProfileDownloaderDelegate::FailureReason reason) {
  // Ignore failures; proceed without the custodian's name.
  download_callback_.Reset();
  profile_downloader_.reset();
}
