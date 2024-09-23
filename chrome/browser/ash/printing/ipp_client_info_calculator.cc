// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/ipp_client_info_calculator.h"

#include <cstdint>
#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/version_info/version_info.h"
#include "printing/mojom/print.mojom.h"

namespace ash::printing {

namespace {

using ::printing::mojom::IppClientInfo;
using ::printing::mojom::IppClientInfoPtr;

constexpr char kOsInfoClientName[] = "ChromeOS";
constexpr char kDeviceDirectoryApiIdPlaceholder[] =
    "${DEVICE_DIRECTORY_API_ID}";
constexpr char kDeviceSerialNumberPlaceholder[] = "${DEVICE_SERIAL_NUMBER}";
constexpr char kDeviceAssetIdPlaceholder[] = "${DEVICE_ASSET_ID}";
constexpr char kDeviceAnnotatedLocationPlaceholder[] =
    "${DEVICE_ANNOTATED_LOCATION}";

// Replace device variables found in `str` with the provided values.
std::string ReplaceDeviceVariables(std::string template_with_vars,
                                   std::string_view api_id,
                                   std::string_view serial,
                                   std::string_view asset_id,
                                   std::string_view location) {
  base::ReplaceSubstringsAfterOffset(&template_with_vars, 0,
                                     kDeviceDirectoryApiIdPlaceholder, api_id);
  base::ReplaceSubstringsAfterOffset(&template_with_vars, 0,
                                     kDeviceSerialNumberPlaceholder, serial);
  base::ReplaceSubstringsAfterOffset(&template_with_vars, 0,
                                     kDeviceAssetIdPlaceholder, asset_id);
  base::ReplaceSubstringsAfterOffset(
      &template_with_vars, 0, kDeviceAnnotatedLocationPlaceholder, location);
  return template_with_vars;
}

class IppClientInfoCalculatorImpl : public IppClientInfoCalculator {
 public:
  IppClientInfoCalculatorImpl(
      std::unique_ptr<policy::DeviceAttributes> device_attributes,
      const std::string& chrome_milestone)
      : cros_settings_(CrosSettings::Get()),
        device_attributes_(std::move(device_attributes)) {
    // Calculate the initial client-info values. The admin-configured
    // `client-info` value with device information can change later if the
    // `kDevicePrintingClientNameTemplate` setting changes.
    CalculateClientInfoWithDeviceValues();
    CalculateClientInfoWithOSVersion(chrome_milestone);

    // Recalculate `client-info` with device information when
    // `kDevicePrintingClientNameTemplate` setting changes.
    // base::Unretained(this) is safe here because the subscription is scoped to
    // the lifetime of `this` already.
    auto callback = base::BindRepeating(
        &IppClientInfoCalculatorImpl::CalculateClientInfoWithDeviceValues,
        base::Unretained(this));
    client_name_template_subscription_ =
        CrosSettings::Get()->AddSettingsObserver(
            kDevicePrintingClientNameTemplate, std::move(callback));
  }

 private:
  IppClientInfoPtr GetOsInfo() const override {
    DCHECK(os_info_);
    return os_info_.Clone();
  }

  IppClientInfoPtr GetDeviceInfo() const override {
    return device_info_.Clone();
  }

  void CalculateClientInfoWithDeviceValues() {
    std::string client_name_template = GetClientNameTemplateFromCrosSettings();
    if (client_name_template.empty()) {
      return;
    }
    std::string serial = device_attributes_->GetDeviceSerialNumber();
    std::string asset_id = device_attributes_->GetDeviceAssetID();
    std::string location = device_attributes_->GetDeviceAnnotatedLocation();
    std::string directory_api_id = device_attributes_->GetDirectoryApiID();
    std::string client_name =
        ReplaceDeviceVariables(std::move(client_name_template),
                               directory_api_id, serial, asset_id, location);
    device_info_ = IppClientInfo::New(IppClientInfo::ClientType::kOther,
                                      std::move(client_name),
                                      /*client_patches=*/std::nullopt,
                                      /*client_string_version=*/std::string(),
                                      /*client_version=*/std::nullopt);
  }

  void CalculateClientInfoWithOSVersion(const std::string& chrome_milestone) {
    DCHECK(base::SysInfo::IsRunningOnChromeOS());
    os_info_ = IppClientInfo::New(
        IppClientInfo::ClientType::kOperatingSystem, kOsInfoClientName,
        base::SysInfo::OperatingSystemVersion(), chrome_milestone,
        /*client_version=*/std::nullopt);
  }

  std::string GetClientNameTemplateFromCrosSettings() {
    std::string client_name_template;
    cros_settings_->GetString(kDevicePrintingClientNameTemplate,
                              &client_name_template);
    return client_name_template;
  }

  raw_ptr<const CrosSettings> cros_settings_;
  std::unique_ptr<policy::DeviceAttributes> device_attributes_;
  IppClientInfoPtr device_info_;
  IppClientInfoPtr os_info_;
  base::CallbackListSubscription client_name_template_subscription_;
};

}  // namespace

std::unique_ptr<IppClientInfoCalculator> IppClientInfoCalculator::Create() {
  return std::make_unique<IppClientInfoCalculatorImpl>(
      std::make_unique<policy::DeviceAttributesImpl>(),
      version_info::GetMajorVersionNumber());
}

std::unique_ptr<IppClientInfoCalculator>
IppClientInfoCalculator::CreateForTesting(
    std::unique_ptr<policy::DeviceAttributes> device_attributes,
    const std::string& chrome_milestone) {
  return std::make_unique<IppClientInfoCalculatorImpl>(
      std::move(device_attributes), chrome_milestone);
}

}  // namespace ash::printing
