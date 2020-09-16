// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_delegate_impl.h"

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"

NearbyShareDelegateImpl::NearbyShareDelegateImpl() = default;

NearbyShareDelegateImpl::~NearbyShareDelegateImpl() = default;

bool NearbyShareDelegateImpl::IsPodButtonVisible() const {
  return GetService() != nullptr;
}

bool NearbyShareDelegateImpl::IsHighVisibilityOn() const {
  NearbySharingService* service = GetService();
  return service && service->IsInHighVisibility();
}

base::Optional<base::TimeDelta>
NearbyShareDelegateImpl::RemainingHighVisibilityTime() const {
  if (!IsHighVisibilityOn())
    return base::nullopt;

  return shutoff_time_ - base::TimeTicks::Now();
}

void NearbyShareDelegateImpl::EnableHighVisibility() {
  NOTIMPLEMENTED();
}

void NearbyShareDelegateImpl::DisableHighVisibility() {
  NOTIMPLEMENTED();
}

NearbySharingService* NearbyShareDelegateImpl::GetService() const {
  return NearbySharingServiceFactory::GetForBrowserContext(
      ProfileManager::GetPrimaryUserProfile());
}

void NearbyShareDelegateImpl::ShowNearbyShareSettings() const {
  settings_opener_->ShowSettingsPage(
      chromeos::settings::mojom::kNearbyShareSubpagePath);
}

void NearbyShareDelegateImpl::SettingsOpener::ShowSettingsPage(
    const std::string& sub_page) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(), sub_page);
}
