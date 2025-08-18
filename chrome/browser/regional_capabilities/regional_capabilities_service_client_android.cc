// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_callback.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/variations/service/variations_service.h"

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
  auto wrapper_callback = base::BindOnce(
      [](CountryIdCallback original_callback,
         const jni_zero::JavaRef<jobject>& j_result) {
        JNIEnv* env = jni_zero::AttachCurrentThread();
        if (!j_result) {
          return;
        }
        jstring j_device_country = static_cast<jstring>(j_result.obj());
        std::string device_country_code = base::ToUpperASCII(
            base::android::ConvertJavaStringToUTF8(env, j_device_country));
        std::move(original_callback).Run(CountryId(device_country_code));
      },
      std::move(on_country_id_fetched));

  Java_RegionalCapabilitiesServiceClientAndroid_requestDeviceCountry(
      base::android::AttachCurrentThread(),
      base::android::ToJniCallback(base::android::AttachCurrentThread(),
                                   std::move(wrapper_callback)));
}

Program RegionalCapabilitiesServiceClientAndroid::GetDeviceProgram() {
  return static_cast<Program>(
      Java_RegionalCapabilitiesServiceClientAndroid_getDeviceProgram(
          jni_zero::AttachCurrentThread()));
}

}  // namespace regional_capabilities
