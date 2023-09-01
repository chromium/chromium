// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/companion_app/companion_app_broker_impl.h"

#include <set>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "url/gurl.h"

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

  const auto metadata_id = device->metadata_id();
  std::set<std::string> target_ids{
      "08A97F", "5A36A5", "6EDAF7", "9ADB11", "A7D7A0", "C8E228",
      "D87A3E", "F2020E", "F58DE7", "30346C", "7862CE",
  };

  if (!target_ids.contains(metadata_id)) {
    return false;
  }

  for (auto& observer : observers_) {
    observer.ShowLaunchCompanionApp(device);
  }
  return true;
}

// TODO(b/274973687): Implement this function
void CompanionAppBrokerImpl::InstallCompanionApp(scoped_refptr<Device> device) {
}

void CompanionAppBrokerImpl::LaunchCompanionApp(scoped_refptr<Device> device) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  const std::string& app_id = ash::features::kFastPairPwaCompanionAppId.Get();
  if (QuickPairBrowserDelegate::Get()->CompanionAppInstalled(app_id)) {
    QuickPairBrowserDelegate::Get()->LaunchCompanionApp(app_id);
  } else {
    NewWindowDelegate::GetPrimary()->OpenUrl(
        GURL(ash::features::kFastPairPwaCompanionInstallUri.Get()),
        NewWindowDelegate::OpenUrlFrom::kUserInteraction,
        NewWindowDelegate::Disposition::kNewForegroundTab);
  }
}

}  // namespace quick_pair
}  // namespace ash
