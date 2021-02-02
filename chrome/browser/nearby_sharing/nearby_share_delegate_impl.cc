// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_delegate_impl.h"

#include "ash/public/cpp/nearby_share_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"

namespace {

const char kStartOnboardingQueryParam[] = "onboarding";
const char kStartReceivingQueryParam[] = "receive";

constexpr base::TimeDelta kShutoffTimeout = base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kOnboardingWaitTimeout =
    base::TimeDelta::FromMinutes(5);

std::string GetTimestampString() {
  return base::NumberToString(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
}

}  // namespace

NearbyShareDelegateImpl::NearbyShareDelegateImpl(
    ash::NearbyShareController* nearby_share_controller)
    : nearby_share_controller_(nearby_share_controller),
      settings_opener_(std::make_unique<SettingsOpener>()),
      shutoff_timer_(
          FROM_HERE,
          kShutoffTimeout,
          base::BindRepeating(&NearbyShareDelegateImpl::DisableHighVisibility,
                              base::Unretained(this))),
      onboarding_wait_timer_(FROM_HERE,
                             kOnboardingWaitTimeout,
                             base::BindRepeating([]() {})) {
  ash::SessionController::Get()->AddObserver(this);
}

NearbyShareDelegateImpl::~NearbyShareDelegateImpl() {
  ash::SessionController::Get()->RemoveObserver(this);
  if (nearby_share_service_)
    RemoveNearbyShareServiceObservers();
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

  // Automatically enable the feature if onboarding is already completed.
  if (nearby_share_service_->GetSettings()->IsOnboardingComplete())
    nearby_share_service_->GetSettings()->SetEnabled(true);

  settings_opener_->ShowSettingsPage(kStartReceivingQueryParam);

  if (!nearby_share_service_->GetSettings()->GetEnabled()) {
    onboarding_wait_timer_.Reset();
  }

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

  if (nearby_share_service_)
    AddNearbyShareServiceObservers();
}

void NearbyShareDelegateImpl::SetNearbyShareServiceForTest(
    NearbySharingService* service) {
  nearby_share_service_ = service;
  AddNearbyShareServiceObservers();
}

void NearbyShareDelegateImpl::AddNearbyShareServiceObservers() {
  DCHECK(nearby_share_service_);
  DCHECK(!nearby_share_service_->HasObserver(this));
  DCHECK(!settings_receiver_.is_bound());
  nearby_share_service_->AddObserver(this);
  nearby_share_service_->GetSettings()->AddSettingsObserver(
      settings_receiver_.BindNewPipeAndPassRemote());
}

void NearbyShareDelegateImpl::RemoveNearbyShareServiceObservers() {
  DCHECK(nearby_share_service_);
  DCHECK(nearby_share_service_->HasObserver(this));
  nearby_share_service_->RemoveObserver(this);
  settings_receiver_.reset();
}

void NearbyShareDelegateImpl::OnEnabledChanged(bool enabled) {
  if (enabled && onboarding_wait_timer_.IsRunning()) {
    onboarding_wait_timer_.Stop();
    EnableHighVisibility();
  }
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
