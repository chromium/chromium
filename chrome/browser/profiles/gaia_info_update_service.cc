// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace {

// Update the user's GAIA info every 24 hours.
const int kUpdateIntervalHours = 24;

// If the users's GAIA info is very out of date then wait at least this long
// before starting an update. This avoids slowdown during startup.
const int kMinUpdateIntervalSeconds = 5;

}  // namespace

GAIAInfoUpdateService::GAIAInfoUpdateService(Profile* profile)
    : profile_(profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  identity_manager->AddObserver(this);

  PrefService* prefs = profile_->GetPrefs();
  last_updated_ = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kProfileGAIAInfoUpdateTime));

  // TODO(msalama): Once Unconsented primary account is available on startup,
  // remove the wait on refresh tokens.
  if (identity_manager->AreRefreshTokensLoaded())
    OnRefreshTokensLoaded();
}

GAIAInfoUpdateService::~GAIAInfoUpdateService() {
  DCHECK(!profile_) << "Shutdown not called before dtor";
}

void GAIAInfoUpdateService::Update() {
  // The user must be logged in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager->HasUnconsentedPrimaryAccount())
    return;

  if (!base::FeatureList::IsEnabled(kPersistUPAInProfileInfoCache) &&
      !identity_manager->HasPrimaryAccount())
    return;

  if (profile_image_downloader_)
    return;
  profile_image_downloader_.reset(new ProfileDownloader(this));
  profile_image_downloader_->Start();
}

// static
bool GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(Profile* profile) {
#if defined(OS_CHROMEOS)
  return false;
#endif
  return true;
}

bool GAIAInfoUpdateService::NeedsProfilePicture() const {
  return true;
}

int GAIAInfoUpdateService::GetDesiredImageSideLength() const {
  return 256;
}

signin::IdentityManager* GAIAInfoUpdateService::GetIdentityManager() {
  return IdentityManagerFactory::GetForProfile(profile_);
}

network::mojom::URLLoaderFactory* GAIAInfoUpdateService::GetURLLoaderFactory() {
  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetURLLoaderFactoryForBrowserProcess()
      .get();
}

std::string GAIAInfoUpdateService::GetCachedPictureURL() const {
  return profile_->GetPrefs()->GetString(prefs::kProfileGAIAInfoPictureURL);
}

bool GAIAInfoUpdateService::IsPreSignin() const {
  return false;
}

void GAIAInfoUpdateService::OnProfileDownloadSuccess(
    ProfileDownloader* downloader) {
  // Make sure that |ProfileDownloader| gets deleted after return.
  std::unique_ptr<ProfileDownloader> profile_image_downloader(
      profile_image_downloader_.release());

  // Save the last updated time.
  last_updated_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(prefs::kProfileGAIAInfoUpdateTime,
                                 last_updated_.ToInternalValue());
  ScheduleNextUpdate();

  base::string16 full_name = downloader->GetProfileFullName();
  base::string16 given_name = downloader->GetProfileGivenName();
  SkBitmap bitmap = downloader->GetProfilePicture();
  ProfileDownloader::PictureStatus picture_status =
      downloader->GetProfilePictureStatus();
  std::string picture_url = downloader->GetProfilePictureURL();
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    return;
  }

  entry->SetGAIAGivenName(given_name);
  entry->SetGAIAName(full_name);

  if (picture_status == ProfileDownloader::PICTURE_SUCCESS) {
    profile_->GetPrefs()->SetString(prefs::kProfileGAIAInfoPictureURL,
                                    picture_url);
    gfx::Image gfx_image = gfx::Image::CreateFrom1xBitmap(bitmap);
    entry->SetGAIAPicture(gfx_image);
  } else if (picture_status == ProfileDownloader::PICTURE_DEFAULT) {
    entry->SetGAIAPicture(gfx::Image());
  }

  const base::string16 hosted_domain = downloader->GetProfileHostedDomain();
  profile_->GetPrefs()->SetString(
      prefs::kGoogleServicesHostedDomain,
      (hosted_domain.empty() ? kNoHostedDomainFound
                             : base::UTF16ToUTF8(hosted_domain)));
}

void GAIAInfoUpdateService::OnProfileDownloadFailure(
    ProfileDownloader* downloader,
    ProfileDownloaderDelegate::FailureReason reason) {
  profile_image_downloader_.reset();

  // Save the last updated time.
  last_updated_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(prefs::kProfileGAIAInfoUpdateTime,
                                 last_updated_.ToInternalValue());
  ScheduleNextUpdate();
}

void GAIAInfoUpdateService::OnUsernameChanged(const std::string& username) {
  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    return;
  }

  if (username.empty()) {
    // Unset the old user's GAIA info.
    entry->SetGAIAName(base::string16());
    entry->SetGAIAGivenName(base::string16());
    entry->SetGAIAPicture(gfx::Image());
    // Unset the cached URL.
    profile_->GetPrefs()->ClearPref(prefs::kProfileGAIAInfoPictureURL);
  } else {
    // Update the new user's GAIA info.
    Update();
  }
}

void GAIAInfoUpdateService::Shutdown() {
  timer_.Stop();
  profile_image_downloader_.reset();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  identity_manager->RemoveObserver(this);

  // OK to reset |profile_| pointer here because GAIAInfoUpdateService will not
  // access it again.  This pointer is also used to implement the delegate for
  // |profile_image_downloader_|.  However that object was destroyed above.
  profile_ = nullptr;
}

void GAIAInfoUpdateService::ScheduleNextUpdate() {
  if (timer_.IsRunning())
    return;

  const base::TimeDelta desired_delta =
      base::TimeDelta::FromHours(kUpdateIntervalHours);
  const base::TimeDelta update_delta = base::Time::Now() - last_updated_;

  base::TimeDelta delta;
  if (update_delta < base::TimeDelta() || update_delta > desired_delta)
    delta = base::TimeDelta::FromSeconds(kMinUpdateIntervalSeconds);
  else
    delta = desired_delta - update_delta;

  // UMA Profile Metrics should be logged regularly.  Logging is not performed
  // in Update() because it is a public method and may be called at any time.
  // These metrics should logged only on this schedule.
  //
  // In mac perf tests, the browser process pointer may be null.
  if (g_browser_process)
    ProfileMetrics::LogNumberOfProfiles(g_browser_process->profile_manager());

  timer_.Start(FROM_HERE, delta, this, &GAIAInfoUpdateService::Update);
}

void GAIAInfoUpdateService::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  OnUsernameChanged(primary_account_info.gaia);
}

void GAIAInfoUpdateService::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  OnUsernameChanged(std::string());
}

void GAIAInfoUpdateService::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  if (identity_manager->HasPrimaryAccount() ||
      !base::FeatureList::IsEnabled(kPersistUPAInProfileInfoCache)) {
    return;
  }

  OnUsernameChanged(unconsented_primary_account_info.gaia);
}

void GAIAInfoUpdateService::OnRefreshTokensLoaded() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  bool clear_profile =
      !identity_manager->HasUnconsentedPrimaryAccount() ||
      (!base::FeatureList::IsEnabled(kPersistUPAInProfileInfoCache) &&
       !identity_manager->HasPrimaryAccount());

  if (clear_profile) {
    OnUsernameChanged(std::string());
  }

  ScheduleNextUpdate();
}
