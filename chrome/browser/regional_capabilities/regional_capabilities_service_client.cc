// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"

#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/variations/service/variations_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/regional_capabilities/android/jni_headers/RegionalCapabilitiesServiceClientAndroid_jni.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

using ::country_codes::CountryId;

namespace regional_capabilities {
namespace {
#if BUILDFLAG(IS_CHROMEOS)
std::optional<CountryId> GetVpdCountry() {
  using enum ChromeOSFallbackCountry;

  ash::system::StatisticsProvider* sys_info =
      ash::system::StatisticsProvider::GetInstance();
  if (!sys_info) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData,
                                  kNoStatisticsProvider);
    return {};
  }

  if (sys_info->GetLoadingState() !=
      ash::system::StatisticsProvider::LoadingState::kFinished) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData,
                                  kStatisticsLoadingNotFinished);
    return {};
  }

  std::optional<std::string_view> maybe_vpd_region =
      sys_info->GetMachineStatistic(ash::system::kRegionKey);
  if (!maybe_vpd_region.has_value()) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionAbsent);
    return {};
  }

  std::string vpd_region = base::ToUpperASCII(maybe_vpd_region.value());
  if (vpd_region.empty()) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionEmpty);
    return {};
  }
  if (vpd_region == "GCC" || vpd_region == "LATAM-ES-419" ||
      vpd_region == "NORDIC") {
    // TODO: crbug.com/377475851 - Implement a lookup for the groupings.
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kGroupedRegion);
    return {};
  }
  if (vpd_region.size() < 2) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionTooShort);
    return {};
  }

  bool has_stripped_subkey_info = false;
  if (vpd_region.size() > 2) {
    if (vpd_region[2] != '.') {
      base::UmaHistogramEnumeration(kCrOSMissingVariationData, kRegionTooLong);
      return {};
    }
    vpd_region = vpd_region.substr(0, 2);
    has_stripped_subkey_info = true;
  }

  const CountryId country_code(vpd_region);
  if (has_stripped_subkey_info) {
    base::UmaHistogramBoolean(kVpdRegionSplittingOutcome,
                              country_code.IsValid());
  }
  if (!country_code.IsValid()) {
    base::UmaHistogramEnumeration(kCrOSMissingVariationData,
                                  kInvalidCountryCode);
    return {};
  }

  base::UmaHistogramEnumeration(kCrOSMissingVariationData, kValidCountryCode);
  return country_code;
}
#endif
}  // namespace

RegionalCapabilitiesServiceClient::RegionalCapabilitiesServiceClient(
    variations::VariationsService* variations_service)
    : variations_latest_country_id_(
          variations_service ? CountryId(base::ToUpperASCII(
                                   variations_service->GetLatestCountry()))
                             : CountryId()) {}

RegionalCapabilitiesServiceClient::~RegionalCapabilitiesServiceClient() =
    default;

CountryId RegionalCapabilitiesServiceClient::GetVariationsLatestCountryId() {
  return variations_latest_country_id_;
}

CountryId RegionalCapabilitiesServiceClient::GetFallbackCountryId() {
#if BUILDFLAG(IS_CHROMEOS)
  if (const std::optional<CountryId> vpd_country = GetVpdCountry();
      vpd_country.has_value()) {
    return *vpd_country;
  }
#endif
  return country_codes::GetCurrentCountryID();
}

#if BUILDFLAG(IS_ANDROID)
void RegionalCapabilitiesServiceClient::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
  // On Android get it from a device API in Java.
  // Usage of `WeakPtr` is crucial here, as `RegionalCapabilitiesService` is
  // not guaranteed to be alive when the response from Java arrives.
  auto heap_callback =
      std::make_unique<CountryIdCallback>(std::move(on_country_id_fetched));
  // The ownership of the callback on the heap is passed to Java. It will be
  // deleted by JNI_RegionalCapabilitiesService_ProcessDeviceCountryResponse.
  Java_RegionalCapabilitiesServiceClientAndroid_requestDeviceCountry(
      base::android::AttachCurrentThread(),
      reinterpret_cast<intptr_t>(heap_callback.release()));
}
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
void RegionalCapabilitiesServiceClient::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
  std::move(on_country_id_fetched).Run(variations_latest_country_id_);
}
#else
// On other platforms, defer to `GetCurrentCountryID()`.
void RegionalCapabilitiesServiceClient::FetchCountryId(
    CountryIdCallback on_country_id_fetched) {
  std::move(on_country_id_fetched).Run(country_codes::GetCurrentCountryID());
}
#endif

#if BUILDFLAG(IS_ANDROID)
void JNI_RegionalCapabilitiesServiceClientAndroid_ProcessDeviceCountryResponse(
    JNIEnv* env,
    jlong ptr_to_native_callback,
    const base::android::JavaParamRef<jstring>& j_device_country) {
  // Using base::WrapUnique ensures that the callback is deleted when this goes
  // out of scope.
  using CountryIdCallback =
      RegionalCapabilitiesService::Client::CountryIdCallback;
  std::unique_ptr<CountryIdCallback> heap_callback = base::WrapUnique(
      reinterpret_cast<CountryIdCallback*>(ptr_to_native_callback));
  CHECK(heap_callback);
  if (!j_device_country) {
    return;
  }
  std::string device_country_code = base::ToUpperASCII(
      base::android::ConvertJavaStringToUTF8(env, j_device_country));
  std::move(*heap_callback).Run(CountryId(device_country_code));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace regional_capabilities
