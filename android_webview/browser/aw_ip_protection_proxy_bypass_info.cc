// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_ip_protection_proxy_bypass_info.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/ExclusionUtilities_jni.h"

namespace android_webview {

std::vector<std::string> LoadExclusionList() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> jobject_domains;
  switch (static_cast<WebviewExclusionPolicy>(
      features::kWebViewIpProtectionExclusionCriteria.Get())) {
    case WebviewExclusionPolicy::kNone: {
      return std::vector<std::string>();
    }
    case WebviewExclusionPolicy::kAndroidAssetStatements: {
      jobject_domains = exclusion_utilities::
          Java_ExclusionUtilities_getDomainsFromAssetStatements(env);
      break;
    }
    case WebviewExclusionPolicy::kAndroidVerifiedAppLinks: {
      jobject_domains = exclusion_utilities::
          Java_ExclusionUtilities_getVerifiedDomainsFromAppLinks(env);
      break;
    }
    case WebviewExclusionPolicy::kAndroidWebLinks: {
      jobject_domains =
          exclusion_utilities::Java_ExclusionUtilities_getDomainsFromWebLinks(
              env);
      break;
    }
    case WebviewExclusionPolicy::kAndroidAssetStatementsAndWebLinks: {
      jobject_domains = exclusion_utilities::
          Java_ExclusionUtilities_getDomainsFromAssetStatementsAndWebLinks(env);
      break;
    }
    default: {
      return std::vector<std::string>();
    }
  }
  std::vector<std::string> domains_to_exclude;
  base::android::AppendJavaStringArrayToStringVector(env, jobject_domains,
                                                     &domains_to_exclude);
  return domains_to_exclude;
}

}  // namespace android_webview
