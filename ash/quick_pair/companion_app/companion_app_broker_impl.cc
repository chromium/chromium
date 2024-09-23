// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/companion_app/companion_app_broker_impl.h"

#include <algorithm>
#include <set>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/login_status.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/cross_device/logging/logging.h"

namespace {

bool IsLoggedIn(ash::LoginStatus status) {
  switch (status) {
    case ash::LoginStatus::NOT_LOGGED_IN:
    case ash::LoginStatus::LOCKED:
    case ash::LoginStatus::KIOSK_APP:
    case ash::LoginStatus::GUEST:
    case ash::LoginStatus::PUBLIC:
      return false;
    case ash::LoginStatus::USER:
    case ash::LoginStatus::CHILD:
    default:
      return true;
  }
}

}  // namespace

namespace ash {
namespace quick_pair {

CompanionAppBrokerImpl::CompanionAppBrokerImpl() {}

CompanionAppBrokerImpl::~CompanionAppBrokerImpl() = default;

void CompanionAppBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CompanionAppBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool CompanionAppBrokerImpl::MaybeShowCompanionAppActions(
    scoped_refptr<Device> device) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  // TODO(b/274973687): Make this logic more generalized once metadata
  // includes companion app ID.
  const auto metadata_id = device->metadata_id();

  const std::string& device_ids =
      ash::features::kFastPairPwaCompanionDeviceIds.Get();

  std::set<std::string> target_ids{
      "08A97F", "5A36A5", "6EDAF7", "9ADB11", "A7D7A0", "C8E228",
      "D87A3E", "F2020E", "F58DE7", "30346C", "7862CE",
  };

  if (!target_ids.contains(metadata_id) &&
      device_ids.find(metadata_id) == std::string::npos) {
    return false;
  }

  bool has_browser_link =
      GURL(ash::features::kFastPairPwaCompanionInstallUri.Get()).is_valid();
  bool has_play_link =
      GURL(ash::features::kFastPairPwaCompanionPlayStoreUri.Get()).is_valid();
  bool companion_installed =
      QuickPairBrowserDelegate::Get()->CompanionAppInstalled(
          ash::features::kFastPairPwaCompanionAppId.Get());

  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  bool is_guest =
      !identity_manager ||
      !IsLoggedIn(Shell::Get()->session_controller()->login_status());

  // Do not show notification if there is no way to install or open the app.
  if (((!has_play_link && !companion_installed) || is_guest) &&
      !has_browser_link) {
    return false;
  }

  // Skip to opening the companion directly if no Play store link is available,
  // the app is already installed, or we are in guest-mode (guests cannot
  // install apps).
  if (!has_play_link || companion_installed || is_guest) {
    CD_LOG(VERBOSE, Feature::FP) << __func__
                                 << ": Showing \"Launch companion app\" "
                                    "notification.";

    for (auto& observer : observers_) {
      observer.ShowLaunchCompanionApp(device);
    }
  } else {
    for (auto& observer : observers_) {
      observer.ShowInstallCompanionApp(device);
    }
  }

  return true;
}

void CompanionAppBrokerImpl::InstallCompanionApp(scoped_refptr<Device> device) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Opening play store page for companion app.";

  // TODO(b/274973687): Make more generalized once device metadata includes link
  QuickPairBrowserDelegate::Get()->OpenPlayStorePage(
      GURL(ash::features::kFastPairPwaCompanionPlayStoreUri.Get()));
}

void CompanionAppBrokerImpl::LaunchCompanionApp(scoped_refptr<Device> device) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  const std::string& app_id = ash::features::kFastPairPwaCompanionAppId.Get();
  if (QuickPairBrowserDelegate::Get()->CompanionAppInstalled(app_id)) {
    CD_LOG(VERBOSE, Feature::FP) << __func__
                                 << ": Found installed app. Launching "
                                    "companion app.";

    QuickPairBrowserDelegate::Get()->LaunchCompanionApp(app_id);
  } else {
    CD_LOG(VERBOSE, Feature::FP) << __func__
                                 << ": No Play store link or installed app. "
                                    "Opening companion web page.";

    NewWindowDelegate::GetPrimary()->OpenUrl(
        GURL(ash::features::kFastPairPwaCompanionInstallUri.Get()),
        NewWindowDelegate::OpenUrlFrom::kUserInteraction,
        NewWindowDelegate::Disposition::kNewForegroundTab);
  }
}

}  // namespace quick_pair
}  // namespace ash
