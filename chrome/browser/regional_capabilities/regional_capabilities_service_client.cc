// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"

#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "components/country_codes/country_codes.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "components/variations/service/variations_service.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/regional_capabilities/android/jni_headers/RegionalCapabilitiesServiceClientAndroid_jni.h"
#endif

namespace regional_capabilities {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

RegionalCapabilitiesServiceClient::RegionalCapabilitiesServiceClient(
    variations::VariationsService* variations_service)
    : variations_country_id_(
          variations_service
              ? country_codes::CountryStringToCountryID(
                    base::ToUpperASCII(variations_service->GetLatestCountry()))
              : country_codes::kCountryIDUnknown) {}
#else
RegionalCapabilitiesServiceClient::RegionalCapabilitiesServiceClient() =
    default;

#endif

RegionalCapabilitiesServiceClient::~RegionalCapabilitiesServiceClient() =
    default;

int RegionalCapabilitiesServiceClient::GetFallbackCountryId() {
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
  std::move(on_country_id_fetched).Run(variations_country_id_);
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
  std::string device_country =
      base::android::ConvertJavaStringToUTF8(env, j_device_country);
  int device_country_id =
      country_codes::CountryStringToCountryID(device_country);
  std::move(*heap_callback).Run(device_country_id);
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace regional_capabilities
