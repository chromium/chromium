// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_attributes_ash.h"

#include <utility>

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/hostname_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/user.h"

namespace crosapi {

namespace {

const char kAccessDenied[] = "Access denied.";

// Whether device attributes can be accessed for the current profile.
bool CanGetDeviceAttributes() {
  const Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return true;

  if (!profile->IsRegularProfile())
    return false;

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  return user->IsAffiliated();
}

}  // namespace

DeviceAttributesAsh::DeviceAttributesAsh() = default;
DeviceAttributesAsh::~DeviceAttributesAsh() = default;

void DeviceAttributesAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeviceAttributes> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceAttributesAsh::GetDirectoryDeviceId(
    GetDirectoryDeviceIdCallback callback) {
  if (!CanGetDeviceAttributes()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
    return;
  }
  std::string result = g_browser_process->platform_part()
                           ->browser_policy_connector_chromeos()
                           ->GetDirectoryApiID();
  if (result.empty()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(result));
  }
}

void DeviceAttributesAsh::GetDeviceSerialNumber(
    GetDeviceSerialNumberCallback callback) {
  if (!CanGetDeviceAttributes()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
    return;
  }
  std::string result = chromeos::system::StatisticsProvider::GetInstance()
                           ->GetEnterpriseMachineID();
  if (result.empty()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(result));
  }
}

void DeviceAttributesAsh::GetDeviceAssetId(GetDeviceAssetIdCallback callback) {
  if (!CanGetDeviceAttributes()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
    return;
  }
  std::string result = g_browser_process->platform_part()
                           ->browser_policy_connector_chromeos()
                           ->GetDeviceAssetID();
  if (result.empty()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(result));
  }
}

void DeviceAttributesAsh::GetDeviceAnnotatedLocation(
    GetDeviceAnnotatedLocationCallback callback) {
  if (!CanGetDeviceAttributes()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
    return;
  }
  std::string result = g_browser_process->platform_part()
                           ->browser_policy_connector_chromeos()
                           ->GetDeviceAnnotatedLocation();
  if (result.empty()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(result));
  }
}

void DeviceAttributesAsh::GetDeviceHostname(
    GetDeviceHostnameCallback callback) {
  if (!CanGetDeviceAttributes()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
    return;
  }
  std::string result = g_browser_process->platform_part()
                           ->browser_policy_connector_chromeos()
                           ->GetHostnameHandler()
                           ->GetDeviceHostname();
  if (result.empty()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(result));
  }
}

}  // namespace crosapi
