// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/device_attribute_api.h"

#include "base/functional/callback.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <optional>
#include <string_view>

#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

using blink::mojom::DeviceAPIService;
using blink::mojom::DeviceAttributeResultPtr;

namespace {

using Result = blink::mojom::DeviceAttributeResult;

constexpr char kNotAffiliatedErrorMessage[] =
    "This web API is not allowed if the current profile is not affiliated.";

constexpr char kNoDeviceAttributesPermissionErrorMessage[] =
    "The current origin cannot use this web API because it was not granted the "
    "'device-attributes' permission.";

#if !BUILDFLAG(IS_CHROMEOS)
const char kNotSupportedPlatformErrorMessage[] =
    "This web API is not supported on the current platform.";
#endif

}  // namespace

DeviceAttributeApiImpl::DeviceAttributeApiImpl() = default;
DeviceAttributeApiImpl::~DeviceAttributeApiImpl() = default;

void DeviceAttributeApiImpl::ReportNotAffiliatedError(
    base::OnceCallback<void(DeviceAttributeResultPtr)> callback) {
  std::move(callback).Run(Result::NewErrorMessage(kNotAffiliatedErrorMessage));
}

void DeviceAttributeApiImpl::ReportNotAllowedError(
    base::OnceCallback<void(DeviceAttributeResultPtr)> callback) {
  std::move(callback).Run(
      Result::NewErrorMessage(kNoDeviceAttributesPermissionErrorMessage));
}

void DeviceAttributeApiImpl::GetDirectoryId(
    DeviceAPIService::GetDirectoryIdCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  const std::string attribute = g_browser_process->platform_part()
                                    ->browser_policy_connector_ash()
                                    ->GetDirectoryApiID();
  if (attribute.empty()) {
    std::move(callback).Run(Result::NewAttribute(std::optional<std::string>()));
  } else {
    std::move(callback).Run(Result::NewAttribute(attribute));
  }
#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}

void DeviceAttributeApiImpl::GetHostname(
    DeviceAPIService::GetHostnameCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  const std::optional<std::string> attribute =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceNamePolicyHandler()
          ->GetHostnameChosenByAdministrator();
  std::move(callback).Run(Result::NewAttribute(attribute));
#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}

void DeviceAttributeApiImpl::GetSerialNumber(
    DeviceAPIService::GetSerialNumberCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  const std::optional<std::string_view> attribute =
      ash::system::StatisticsProvider::GetInstance()->GetMachineID();
  std::move(callback).Run(Result::NewAttribute(
      attribute ? std::optional<std::string>(attribute.value())
                : std::nullopt));

#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}

void DeviceAttributeApiImpl::GetAnnotatedAssetId(
    DeviceAPIService::GetAnnotatedAssetIdCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  const std::string attribute = g_browser_process->platform_part()
                                    ->browser_policy_connector_ash()
                                    ->GetDeviceAssetID();
  if (attribute.empty()) {
    std::move(callback).Run(Result::NewAttribute(std::optional<std::string>()));
  } else {
    std::move(callback).Run(Result::NewAttribute(attribute));
  }
#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}

void DeviceAttributeApiImpl::GetAnnotatedLocation(
    DeviceAPIService::GetAnnotatedLocationCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  const std::string attribute = g_browser_process->platform_part()
                                    ->browser_policy_connector_ash()
                                    ->GetDeviceAnnotatedLocation();
  if (attribute.empty()) {
    std::move(callback).Run(Result::NewAttribute(std::optional<std::string>()));
  } else {
    std::move(callback).Run(Result::NewAttribute(attribute));
  }
#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}
