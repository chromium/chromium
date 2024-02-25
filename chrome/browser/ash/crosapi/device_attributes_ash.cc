// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_attributes_ash.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"

namespace crosapi {

namespace {

const char kAccessDenied[] = "Access denied.";

void GetAttribute(
    base::OnceCallback<void(mojom::DeviceAttributesStringResultPtr)> callback,
    const std::string& attr_value) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  if (!browser_util::IsSigninProfileOrBelongsToAffiliatedUser(profile)) {
    std::move(callback).Run(
        mojom::DeviceAttributesStringResult::NewErrorMessage(kAccessDenied));
    return;
  }

  std::move(callback).Run(
      mojom::DeviceAttributesStringResult::NewContents(attr_value));
}

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
  GetAttribute(std::move(callback), attributes_->GetDirectoryApiID());
}

void DeviceAttributesAsh::GetDeviceSerialNumber(
    GetDeviceSerialNumberCallback callback) {
  GetAttribute(std::move(callback), attributes_->GetDeviceSerialNumber());
}

void DeviceAttributesAsh::GetDeviceAssetId(GetDeviceAssetIdCallback callback) {
  GetAttribute(std::move(callback), attributes_->GetDeviceAssetID());
}

void DeviceAttributesAsh::GetDeviceAnnotatedLocation(
    GetDeviceAnnotatedLocationCallback callback) {
  GetAttribute(std::move(callback), attributes_->GetDeviceAnnotatedLocation());
}

void DeviceAttributesAsh::GetDeviceHostname(
    GetDeviceHostnameCallback callback) {
  GetAttribute(std::move(callback),
               attributes_->GetDeviceHostname().value_or(""));
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
