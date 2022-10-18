// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_attributes_ash.h"

#include <string>
#include <utility>

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "components/user_manager/user.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {

namespace {

const char kAccessDenied[] = "Access denied.";

}  // namespace

DeviceAttributesAsh::DeviceAttributesAsh()
    : attributes_(std::make_unique<policy::DeviceAttributesImpl>()) {}

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
  std::string result = attributes_->GetDirectoryApiID();
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
  std::string result = attributes_->GetDeviceSerialNumber();
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
  std::string result = attributes_->GetDeviceAssetID();
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
  std::string result = attributes_->GetDeviceAnnotatedLocation();
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
  absl::optional<std::string> result = attributes_->GetDeviceHostname();
  if (!result) {
    std::move(callback).Run(StringResult::NewErrorMessage(kAccessDenied));
  } else {
    std::move(callback).Run(StringResult::NewContents(*result));
  }
}

void DeviceAttributesAsh::GetDeviceTypeForMetrics(
    GetDeviceTypeForMetricsCallback callback) {
  std::move(callback).Run(apps::GetUserTypeByDeviceTypeMetrics());
}

void DeviceAttributesAsh::SetDeviceAttributesForTesting(
    std::unique_ptr<policy::FakeDeviceAttributes> attributes) {
  attributes_ = std::move(attributes);
}

}  // namespace crosapi
