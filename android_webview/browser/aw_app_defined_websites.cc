// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_app_defined_websites.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AppDefinedDomains_jni.h"

namespace android_webview {

std::vector<std::string>* g_app_defined_domains() {
  // This list of domains doesn't change between app launches.
  // For that reason, it is better to cache it for every navigation.
  static base::NoDestructor<std::vector<std::string>> app_defined_domains(
      GetAppDefinedDomains(
          AppDefinedDomainCriteria::kAndroidAssetStatementsAndWebLinks));

  return app_defined_domains.get();
}

std::vector<std::string> GetAppDefinedDomains(
    AppDefinedDomainCriteria criteria) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> jobject_domains;
  switch (criteria) {
    case AppDefinedDomainCriteria::kNone: {
      return std::vector<std::string>();
    }
    case AppDefinedDomainCriteria::kAndroidAssetStatements: {
      jobject_domains =
          Java_AppDefinedDomains_getDomainsFromAssetStatements(env);
      break;
    }
    case AppDefinedDomainCriteria::kAndroidVerifiedAppLinks: {
      jobject_domains =
          Java_AppDefinedDomains_getVerifiedDomainsFromAppLinks(env);
      break;
    }
    case AppDefinedDomainCriteria::kAndroidWebLinks: {
      jobject_domains = Java_AppDefinedDomains_getDomainsFromWebLinks(env);
      break;
    }
    case AppDefinedDomainCriteria::kAndroidAssetStatementsAndWebLinks: {
      jobject_domains =
          Java_AppDefinedDomains_getDomainsFromAssetStatementsAndWebLinks(env);
      break;
    }
    default: {
      return std::vector<std::string>();
    }
  }
  std::vector<std::string> domains;
  base::android::AppendJavaStringArrayToStringVector(env, jobject_domains,
                                                     &domains);
  return domains;
}

bool IsAppDefined(std::string etld_plus1) {
  auto* app_defined_domains = g_app_defined_domains();

  return std::find_if(
             app_defined_domains->begin(), app_defined_domains->end(),
             [&etld_plus1](const std::string& domain) {
               std::string domain_etld =
                   net::registry_controlled_domains::GetDomainAndRegistry(
                       domain, net::registry_controlled_domains::
                                   INCLUDE_PRIVATE_REGISTRIES);

               return etld_plus1 == domain_etld;
             }) != app_defined_domains->end();
}

}  // namespace android_webview
