// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_delegate_impl.h"

#include "ash/public/cpp/nearby_share_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/session/session_util.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/nearby_sharing/internal/icons/vector_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

const char kStartOnboardingQueryParam[] = "onboarding";
const char kStartReceivingQueryParam[] = "receive";

constexpr base::TimeDelta kShutoffTimeout = base::Minutes(5);

std::string GetTimestampString() {
  return base::NumberToString(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
}

const gfx::VectorIcon kEmptyIcon;

}  // namespace

NearbyShareDelegateImpl::NearbyShareDelegateImpl(
    ash::NearbyShareController* nearby_share_controller)
    : nearby_share_controller_(nearby_share_controller),
      settings_opener_(std::make_unique<SettingsOpener>()),
      shutoff_timer_(
          FROM_HERE,
          kShutoffTimeout,
          base::BindRepeating(&NearbyShareDelegateImpl::DisableHighVisibility,
                              base::Unretained(this))) {
  ash::SessionController::Get()->AddObserver(this);
}

NearbyShareDelegateImpl::~NearbyShareDelegateImpl() {
  ash::SessionController::Get()->RemoveObserver(this);
  if (nearby_share_service_) {
    RemoveNearbyShareServiceObservers();
  }
  nearby_share_settings_receiver_.reset();
}

bool NearbyShareDelegateImpl::IsEnabled() {
  return nearby_share_service_ != nullptr &&
         nearby_share_service_->GetSettings()->GetEnabled();
}

bool NearbyShareDelegateImpl::IsPodButtonVisible() {
  return nearby_share_service_ != nullptr &&
         !nearby_share_service_->GetSettings()->IsDisabledByPolicy();
}

bool NearbyShareDelegateImpl::IsHighVisibilityOn() {
  return nearby_share_service_ && nearby_share_service_->IsInHighVisibility();
}

bool NearbyShareDelegateImpl::IsEnableHighVisibilityRequestActive() const {
  return is_enable_high_visibility_request_active_;
}

base::TimeTicks NearbyShareDelegateImpl::HighVisibilityShutoffTime() const {
  return shutoff_time_;
}

void NearbyShareDelegateImpl::EnableHighVisibility() {
  if (!nearby_share_service_)
    return;

  settings_opener_->ShowSettingsPage(kStartReceivingQueryParam);

  is_enable_high_visibility_request_active_ = true;
}

void NearbyShareDelegateImpl::DisableHighVisibility() {
  if (!nearby_share_service_)
    return;

  shutoff_timer_.Stop();

  nearby_share_service_->ClearForegroundReceiveSurfaces();
}

void NearbyShareDelegateImpl::OnLockStateChanged(bool locked) {
  if (locked && IsHighVisibilityOn()) {
    DisableHighVisibility();
  }
}

void NearbyShareDelegateImpl::OnFirstSessionStarted() {
  nearby_share_service_ = NearbySharingServiceFactory::GetForBrowserContext(
      ProfileManager::GetPrimaryUserProfile());

  if (nearby_share_service_) {
    nearby_share_settings_ = nearby_share_service_->GetSettings();
    AddNearbyShareServiceObservers();
  }
}

void NearbyShareDelegateImpl::SetNearbyShareServiceForTest(
    NearbySharingService* service) {
  nearby_share_service_ = service;
  AddNearbyShareServiceObservers();
}

void NearbyShareDelegateImpl::SetNearbyShareSettingsForTest(
    NearbyShareSettings* settings) {
  nearby_share_settings_ = settings;
}

// In Quick Share v1, not used.
// In Quick Share v2, Quick Share should always be 'enabled', though visibility
// may be set to Hidden.
void NearbyShareDelegateImpl::OnEnabledChanged(bool enabled) {}

void NearbyShareDelegateImpl::OnFastInitiationNotificationStateChanged(
    ::nearby_share::mojom::FastInitiationNotificationState state) {}

void NearbyShareDelegateImpl::OnIsFastInitiationHardwareSupportedChanged(
    bool is_supported) {}

void NearbyShareDelegateImpl::OnDeviceNameChanged(
    const std::string& device_name) {}

void NearbyShareDelegateImpl::OnDataUsageChanged(
    ::nearby_share::mojom::DataUsage data_usage) {}

void NearbyShareDelegateImpl::OnVisibilityChanged(
    ::nearby_share::mojom::Visibility visibility) {
  nearby_share_controller_->VisibilityChanged(visibility);
}

void NearbyShareDelegateImpl::OnAllowedContactsChanged(
    const std::vector<std::string>& visible_contact_ids) {}

void NearbyShareDelegateImpl::OnIsOnboardingCompleteChanged(bool is_complete) {}

void NearbyShareDelegateImpl::AddNearbyShareServiceObservers() {
  DCHECK(nearby_share_service_);
  DCHECK(!nearby_share_service_->HasObserver(this));
  nearby_share_service_->AddObserver(this);
  if (nearby_share_settings_) {
    nearby_share_settings_->AddSettingsObserver(
        nearby_share_settings_receiver_.BindNewPipeAndPassRemote());
  }
}

void NearbyShareDelegateImpl::RemoveNearbyShareServiceObservers() {
  DCHECK(nearby_share_service_);
  DCHECK(nearby_share_service_->HasObserver(this));
  nearby_share_service_->RemoveObserver(this);
}

void NearbyShareDelegateImpl::OnHighVisibilityChangeRequested() {
  is_enable_high_visibility_request_active_ = true;
}

void NearbyShareDelegateImpl::OnHighVisibilityChanged(bool high_visibility_on) {
  is_enable_high_visibility_request_active_ = false;

  if (high_visibility_on) {
    shutoff_time_ = base::TimeTicks::Now() + kShutoffTimeout;
    shutoff_timer_.Reset();
  } else {
    shutoff_timer_.Stop();
  }

  nearby_share_controller_->HighVisibilityEnabledChanged(high_visibility_on);
}

void NearbyShareDelegateImpl::OnShutdown() {
  if (nearby_share_service_) {
    RemoveNearbyShareServiceObservers();
    nearby_share_service_ = nullptr;
  }
}

void NearbyShareDelegateImpl::ShowNearbyShareSettings() const {
  DCHECK(nearby_share_service_);

  std::string query_param =
      nearby_share_service_->GetSettings()->IsOnboardingComplete()
          ? std::string()  // Show settings subpage without dialog.
          : kStartOnboardingQueryParam;  // Show onboarding dialog.
  settings_opener_->ShowSettingsPage(query_param);
}

void NearbyShareDelegateImpl::SettingsOpener::ShowSettingsPage(
    const std::string& sub_page) {
  std::string query_string;
  if (!sub_page.empty()) {
    // Append a timestamp to make the url unique per-call. Otherwise, settings
    // will not respond to successive calls if the url does not change.
    query_string += "?" + sub_page + "&time=" + GetTimestampString();

    if (sub_page == kStartReceivingQueryParam) {
      // Attach high visibility shutoff timeout for display in webui.
      query_string +=
          "&timeout=" + base::NumberToString(kShutoffTimeout.InSeconds());
    }
  }

  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetPrimaryUserProfile(),
      std::string(chromeos::settings::mojom::kNearbyShareSubpagePath) +
          query_string);
}

const gfx::VectorIcon& NearbyShareDelegateImpl::GetIcon(bool on_icon) const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (features::IsNameEnabled()) {
    return on_icon ? kNearbyShareInternalIcon : kNearbyShareInternalOffIcon;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return kEmptyIcon;
}

std::u16string NearbyShareDelegateImpl::GetPlaceholderFeatureName() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (features::IsNameEnabled()) {
    return NearbyShareResourceGetter::GetInstance()->GetFeatureName();
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return u"";
}

::nearby_share::mojom::Visibility NearbyShareDelegateImpl::GetVisibility()
    const {
  if (!nearby_share_settings_) {
    return ::nearby_share::mojom::Visibility::kUnknown;
  }

  return nearby_share_settings_->GetVisibility();
}

void NearbyShareDelegateImpl::SetVisibility(
    ::nearby_share::mojom::Visibility visibility) {
  DCHECK(nearby_share_settings_);
  return nearby_share_settings_->SetVisibility(visibility);
}
