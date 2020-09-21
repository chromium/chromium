// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_delegate_impl.h"

#include "ash/public/cpp/nearby_share_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"

namespace {

const char kStartReceivingQueryParam[] = "receive";
const char kStopReceivingQueryParam[] = "stop_receiving";

constexpr base::TimeDelta kShutoffTimeout = base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kOnboardingWaitTimeout =
    base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kOneMinute = base::TimeDelta::FromSeconds(60);
constexpr base::TimeDelta kOneSecond = base::TimeDelta::FromSeconds(1);

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
          base::BindRepeating(&NearbyShareDelegateImpl::OnShutoffTimerFired,
                              base::Unretained(this))),
      onboarding_wait_timer_(FROM_HERE,
                             kOnboardingWaitTimeout,
                             base::BindRepeating([]() {})) {
  ash::SessionController::Get()->AddObserver(this);
}

NearbyShareDelegateImpl::~NearbyShareDelegateImpl() {
  ash::SessionController::Get()->RemoveObserver(this);
}

bool NearbyShareDelegateImpl::IsPodButtonVisible() {
  return GetService() != nullptr;
}

bool NearbyShareDelegateImpl::IsHighVisibilityOn() {
  NearbySharingService* service = GetService();
  return service && service->IsInHighVisibility();
}

base::Optional<base::TimeDelta>
NearbyShareDelegateImpl::RemainingHighVisibilityTime() {
  if (!IsHighVisibilityOn())
    return base::nullopt;

  return shutoff_time_ - base::TimeTicks::Now();
}

void NearbyShareDelegateImpl::EnableHighVisibility() {
  NearbySharingService* service = GetService();
  if (!service)
    return;

  settings_opener_->ShowSettingsPage(kStartReceivingQueryParam);

  if (!service->GetSettings()->GetEnabled()) {
    onboarding_wait_timer_.Reset();

    if (!settings_receiver_.is_bound()) {
      service->GetSettings()->AddSettingsObserver(
          settings_receiver_.BindNewPipeAndPassRemote());
    }
  }
}

void NearbyShareDelegateImpl::DisableHighVisibility() {
  NearbySharingService* service = GetService();
  if (!service)
    return;

  shutoff_timer_.Stop();
  countdown_timer_.Stop();

  settings_opener_->ShowSettingsPage(kStopReceivingQueryParam);
}

void NearbyShareDelegateImpl::OnLockStateChanged(bool locked) {
  if (locked && IsHighVisibilityOn()) {
    DisableHighVisibility();
  }
}

void NearbyShareDelegateImpl::OnEnabledChanged(bool enabled) {
  if (enabled && onboarding_wait_timer_.IsRunning()) {
    onboarding_wait_timer_.Stop();
    EnableHighVisibility();
  }
}

void NearbyShareDelegateImpl::OnHighVisibilityChanged(bool high_visibility_on) {
  nearby_share_controller_->HighVisibilityEnabledChanged(high_visibility_on);

  if (high_visibility_on) {
    shutoff_time_ = base::TimeTicks::Now() + kShutoffTimeout;
    shutoff_timer_.Reset();

    countdown_timer_.Start(
        FROM_HERE, kOneMinute,
        base::BindRepeating(&NearbyShareDelegateImpl::OnCountdownTimerFired,
                            base::Unretained(this)));
    OnCountdownTimerFired();
  } else {
    shutoff_timer_.Stop();
    countdown_timer_.Stop();
  }
}

void NearbyShareDelegateImpl::OnShutdown() {
  settings_receiver_.reset();
}

void NearbyShareDelegateImpl::OnShutoffTimerFired() {
  DisableHighVisibility();
}

void NearbyShareDelegateImpl::OnCountdownTimerFired() {
  base::Optional<base::TimeDelta> remaining_time =
      RemainingHighVisibilityTime();
  if (!remaining_time)
    return;

  if (countdown_timer_.GetCurrentDelay() > kOneSecond &&
      *remaining_time < kOneMinute) {
    countdown_timer_.Stop();
    countdown_timer_.Start(
        FROM_HERE, kOneSecond,
        base::BindRepeating(&NearbyShareDelegateImpl::OnCountdownTimerFired,
                            base::Unretained(this)));
  }

  nearby_share_controller_->HighVisibilityCountdownUpdate(*remaining_time);
}

NearbySharingService* NearbyShareDelegateImpl::GetService() {
  if (nearby_share_service_for_test_) {
    return nearby_share_service_for_test_;
  }

  return NearbySharingServiceFactory::GetForBrowserContext(
      ProfileManager::GetPrimaryUserProfile());
}

void NearbyShareDelegateImpl::ShowNearbyShareSettings() const {
  settings_opener_->ShowSettingsPage("");
}

void NearbyShareDelegateImpl::SettingsOpener::ShowSettingsPage(
    const std::string& sub_page) {
  std::string query_string;
  if (!sub_page.empty()) {
    // Append a timestamp to make the url unique per-call. Otherwise, settings
    // will not respond to successive calls if the url does not change.
    query_string += "?" + sub_page + "&time=" + GetTimestampString();
  }

  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetPrimaryUserProfile(),
      std::string(chromeos::settings::mojom::kNearbyShareSubpagePath) +
          query_string);
}
