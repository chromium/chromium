// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"
#include "components/country_codes/country_codes.h"
#include "components/variations/service/variations_service.h"
#include "components/regional_capabilities/regional_capabilities_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/regional_capabilities/android/jni_headers/RegionalCapabilitiesServiceClientAndroid_jni.h"

using ::country_codes::CountryId;

namespace regional_capabilities {

RegionalCapabilitiesServiceClientAndroid::
    RegionalCapabilitiesServiceClientAndroid(
        variations::VariationsService* variations_service)
    : RegionalCapabilitiesServiceClient(variations_service) {}

RegionalCapabilitiesServiceClientAndroid::
    ~RegionalCapabilitiesServiceClientAndroid() = default;

void RegionalCapabilitiesServiceClientAndroid::FetchCountryId(
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

static void
JNI_RegionalCapabilitiesServiceClientAndroid_ProcessDeviceCountryResponse(
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

Program RegionalCapabilitiesServiceClientAndroid::GetDeviceProgram() {
  return static_cast<Program>(
      Java_RegionalCapabilitiesServiceClientAndroid_getDeviceProgramForNative(
          jni_zero::AttachCurrentThread()));
}

}  // namespace regional_capabilities

DEFINE_JNI(RegionalCapabilitiesServiceClientAndroid)
