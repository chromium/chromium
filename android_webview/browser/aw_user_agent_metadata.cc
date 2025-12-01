// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_user_agent_metadata.h"

#include <vector>

#include "android_webview/browser/aw_client_hints_controller_delegate.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_number_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwUserAgentMetadata_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::Java2dStringArrayTo2dStringVector;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStringArray;
using base::android::ToJavaArrayOfStrings;

namespace android_webview {

namespace {

}  // namespace
blink::UserAgentMetadata FromJavaAwUserAgentMetadata(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_ua_metadata) {
  blink::UserAgentMetadata ua_metadata;

  std::vector<std::vector<std::string>> brand_version_list;
  ScopedJavaLocalRef<jobjectArray> java_brand_version_list =
      Java_AwUserAgentMetadata_getBrandVersionList(env, java_ua_metadata);
  if (java_brand_version_list) {
    Java2dStringArrayTo2dStringVector(env, java_brand_version_list,
                                      &brand_version_list);
  }

  std::vector<blink::UserAgentBrandVersion> brand_major_version_list;
  std::vector<blink::UserAgentBrandVersion> brand_full_version_list;
  for (const auto& bv : brand_version_list) {
    // java brand version object will include: brand, major_version, and
    // full_version in each brand version item.
    CHECK(bv.size() == 3);

    // brand major version
    brand_major_version_list.emplace_back(bv[0], bv[1]);

    // brand full version
    if (!bv[2].empty()) {
      brand_full_version_list.emplace_back(bv[0], bv[2]);
    }
  }

  // Apply user-agent metadata overrides.
  if (!brand_major_version_list.empty()) {
    ua_metadata.brand_version_list = std::move(brand_major_version_list);
  }

  if (!brand_full_version_list.empty()) {
    ua_metadata.brand_full_version_list = std::move(brand_full_version_list);
  }

  ua_metadata.full_version =
      Java_AwUserAgentMetadata_getFullVersion(env, java_ua_metadata);

  ua_metadata.platform =
      Java_AwUserAgentMetadata_getPlatform(env, java_ua_metadata);

  ua_metadata.platform_version =
      Java_AwUserAgentMetadata_getPlatformVersion(env, java_ua_metadata);

  ua_metadata.architecture =
      Java_AwUserAgentMetadata_getArchitecture(env, java_ua_metadata);

  ua_metadata.model = Java_AwUserAgentMetadata_getModel(env, java_ua_metadata);

  ua_metadata.mobile = Java_AwUserAgentMetadata_isMobile(env, java_ua_metadata);

  std::string bitness = base::NumberToString(
      Java_AwUserAgentMetadata_getBitness(env, java_ua_metadata));
  // Java using int 0 to represent as default empty bitness.
  ua_metadata.bitness = bitness == "0" ? "" : bitness;

  ua_metadata.wow64 = Java_AwUserAgentMetadata_isWow64(env, java_ua_metadata);

  base::android::AppendJavaStringArrayToStringVector(
      env, Java_AwUserAgentMetadata_getFormFactors(env, java_ua_metadata),
      &ua_metadata.form_factors);

  return ua_metadata;
}

ScopedJavaLocalRef<jobject> ToJavaAwUserAgentMetadata(
    JNIEnv* env,
    const blink::UserAgentMetadata& ua_metadata) {
  std::vector<std::vector<std::string>> brand_version_list;
  std::vector<std::vector<std::string>> brand_full_version_list;
  for (const auto& bv : ua_metadata.brand_version_list) {
    brand_version_list.emplace_back(
        std::vector<std::string>{bv.brand, bv.version});
  }
  for (const auto& bv : ua_metadata.brand_full_version_list) {
    brand_full_version_list.emplace_back(
        std::vector<std::string>{bv.brand, bv.version});
  }

  ScopedJavaLocalRef<jobjectArray> java_brand_version_list =
      ToJavaArrayOfStringArray(env, brand_version_list);
  ScopedJavaLocalRef<jobjectArray> java_brand_full_version_lis =
      ToJavaArrayOfStringArray(env, brand_full_version_list);
  jboolean java_mobile = ua_metadata.mobile;
  jboolean java_wow64 = ua_metadata.wow64;
  ScopedJavaLocalRef<jobjectArray> java_form_factors =
      ToJavaArrayOfStrings(env, ua_metadata.form_factors);

  return Java_AwUserAgentMetadata_create(
      env, java_brand_version_list, java_brand_full_version_lis,
      ua_metadata.full_version, ua_metadata.platform,
      ua_metadata.platform_version, ua_metadata.architecture, ua_metadata.model,
      java_mobile, ua_metadata.bitness, java_wow64, java_form_factors);
}

}  // namespace android_webview

DEFINE_JNI(AwUserAgentMetadata)
