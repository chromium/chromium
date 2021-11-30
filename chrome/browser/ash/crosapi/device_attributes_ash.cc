// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_attributes_ash.h"

#include <utility>

#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/user.h"

namespace crosapi {

namespace {

const char kAccessDenied[] = "Access denied.";

}  // namespace

DeviceAttributesAsh::DeviceAttributesAsh() = default;
DeviceAttributesAsh::~DeviceAttributesAsh() = default;

void DeviceAttributesAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeviceAttributes> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceAttributesAsh::GetDirectoryDeviceId(
    GetDirectoryDeviceIdCallback callback) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!browser_util::IsSigninProfileOrBelongsToAffiliatedUser(profile)) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
    return;
  }
  std::string result = g_browser_process->platform_part()
                           ->browser_policy_connector_ash()
                           ->GetDirectoryApiID();
  if (result.empty()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(result));
  }
}

void DeviceAttributesAsh::GetDeviceSerialNumber(
    GetDeviceSerialNumberCallback callback) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!browser_util::IsSigninProfileOrBelongsToAffiliatedUser(profile)) {
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
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!browser_util::IsSigninProfileOrBelongsToAffiliatedUser(profile)) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
    return;
  }
  std::string result = g_browser_process->platform_part()
                           ->browser_policy_connector_ash()
                           ->GetDeviceAssetID();
  if (result.empty()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(result));
  }
}

void DeviceAttributesAsh::GetDeviceAnnotatedLocation(
    GetDeviceAnnotatedLocationCallback callback) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!browser_util::IsSigninProfileOrBelongsToAffiliatedUser(profile)) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
    return;
  }
  std::string result = g_browser_process->platform_part()
                           ->browser_policy_connector_ash()
                           ->GetDeviceAnnotatedLocation();
  if (result.empty()) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(result));
  }
}

void DeviceAttributesAsh::GetDeviceHostname(
    GetDeviceHostnameCallback callback) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!browser_util::IsSigninProfileOrBelongsToAffiliatedUser(profile)) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
    return;
  }
  absl::optional<std::string> result = g_browser_process->platform_part()
                                           ->browser_policy_connector_ash()
                                           ->GetDeviceNamePolicyHandler()
                                           ->GetHostnameChosenByAdministrator();
  if (!result) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(*result));
  }
}

}  // namespace crosapi
