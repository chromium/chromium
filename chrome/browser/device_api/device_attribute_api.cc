// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/device_attribute_api.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/hostname_handler.h"
#include "chromeos/system/statistics_provider.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#endif

namespace device_attribute_api {

namespace {

using Result = blink::mojom::DeviceAttributeResult;

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
const char kNotSupportedPlatformErrorMessage[] =
    "This restricted web API is not supported on the current platform.";
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void AdaptLacrosResult(
    DeviceAPIService::GetDirectoryIdCallback callback,
    crosapi::mojom::DeviceAttributesStringResultPtr lacros_result) {
  if (lacros_result->is_error_message()) {
    std::move(callback).Run(
        Result::NewErrorMessage(lacros_result->get_error_message()));
  } else if (lacros_result->get_contents().empty()) {
    std::move(callback).Run(
        Result::NewAttribute(base::Optional<std::string>()));
  } else {
    std::move(callback).Run(
        Result::NewAttribute(lacros_result->get_contents()));
  }
}
#endif

}  // namespace

void GetDirectoryId(DeviceAPIService::GetDirectoryIdCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::string attribute = g_browser_process->platform_part()
                                    ->browser_policy_connector_chromeos()
                                    ->GetDirectoryApiID();
  if (attribute.empty())
    std::move(callback).Run(
        Result::NewAttribute(base::Optional<std::string>()));
  else
    std::move(callback).Run(Result::NewAttribute(attribute));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDirectoryDeviceId(
          base::BindOnce(AdaptLacrosResult, std::move(callback)));
#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}

void GetHostname(DeviceAPIService::GetHostnameCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::string attribute = g_browser_process->platform_part()
                                    ->browser_policy_connector_chromeos()
                                    ->GetHostnameHandler()
                                    ->GetDeviceHostname();
  if (attribute.empty())
    std::move(callback).Run(
        Result::NewAttribute(base::Optional<std::string>()));
  else
    std::move(callback).Run(Result::NewAttribute(attribute));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDeviceHostname(
          base::BindOnce(AdaptLacrosResult, std::move(callback)));
#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}

void GetSerialNumber(DeviceAPIService::GetSerialNumberCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::string attribute =
      chromeos::system::StatisticsProvider::GetInstance()
          ->GetEnterpriseMachineID();
  if (attribute.empty())
    std::move(callback).Run(
        Result::NewAttribute(base::Optional<std::string>()));
  else
    std::move(callback).Run(Result::NewAttribute(attribute));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDeviceSerialNumber(
          base::BindOnce(AdaptLacrosResult, std::move(callback)));
#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}

void GetAnnotatedAssetId(
    DeviceAPIService::GetAnnotatedAssetIdCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::string attribute = g_browser_process->platform_part()
                                    ->browser_policy_connector_chromeos()
                                    ->GetDeviceAssetID();
  if (attribute.empty())
    std::move(callback).Run(
        Result::NewAttribute(base::Optional<std::string>()));
  else
    std::move(callback).Run(Result::NewAttribute(attribute));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDeviceAssetId(
          base::BindOnce(AdaptLacrosResult, std::move(callback)));
#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}

void GetAnnotatedLocation(
    DeviceAPIService::GetAnnotatedLocationCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::string attribute = g_browser_process->platform_part()
                                    ->browser_policy_connector_chromeos()
                                    ->GetDeviceAnnotatedLocation();
  if (attribute.empty())
    std::move(callback).Run(
        Result::NewAttribute(base::Optional<std::string>()));
  else
    std::move(callback).Run(Result::NewAttribute(attribute));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDeviceAnnotatedLocation(
          base::BindOnce(AdaptLacrosResult, std::move(callback)));
#else  // Other platforms
  std::move(callback).Run(
      Result::NewErrorMessage(kNotSupportedPlatformErrorMessage));
#endif
}

}  // namespace device_attribute_api
