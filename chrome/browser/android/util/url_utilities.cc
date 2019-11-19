// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "chrome/browser/util/jni_headers/UrlUtilities_jni.h"
#include "components/google/core/common/google_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

static const char* const g_supported_schemes[] = {
    "about", "data", "file", "http", "https", "inline", "javascript", nullptr};

static const char* const g_downloadable_schemes[] = {
    "data", "blob", "file", "filesystem", "http", "https", nullptr};

static const char* const g_fallback_valid_schemes[] = {"http", "https",
                                                       nullptr};

GURL JNI_UrlUtilities_ConvertJavaStringToGURL(JNIEnv* env, jstring url) {
  return url ? GURL(ConvertJavaStringToUTF8(env, url)) : GURL();
}

bool CheckSchemeBelongsToList(JNIEnv* env,
                              const JavaParamRef<jstring>& url,
                              const char* const* scheme_list) {
  GURL gurl = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url);
  if (gurl.is_valid()) {
    for (size_t i = 0; scheme_list[i]; i++) {
      if (gurl.scheme() == scheme_list[i]) {
        return true;
      }
    }
  }
  return false;
}

net::registry_controlled_domains::PrivateRegistryFilter GetRegistryFilter(
    jboolean include_private) {
  return include_private
             ? net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES
             : net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
}

}  // namespace

// Returns whether the given URLs have the same domain or host.
// See net::registry_controlled_domains::SameDomainOrHost for details.
static jboolean JNI_UrlUtilities_SameDomainOrHost(
    JNIEnv* env,
    const JavaParamRef<jstring>& url_1_str,
    const JavaParamRef<jstring>& url_2_str,
    jboolean include_private) {
  GURL url_1 = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url_1_str);
  GURL url_2 = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url_2_str);

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return net::registry_controlled_domains::SameDomainOrHost(url_1, url_2,
                                                            filter);
}

// Returns the Domain and Registry of the given URL.
// See net::registry_controlled_domains::GetDomainAndRegistry for details.
static ScopedJavaLocalRef<jstring> JNI_UrlUtilities_GetDomainAndRegistry(
    JNIEnv* env,
    const JavaParamRef<jstring>& url,
    jboolean include_private) {
  DCHECK(url);
  GURL gurl = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return ScopedJavaLocalRef<jstring>();

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return base::android::ConvertUTF8ToJavaString(
      env,
      net::registry_controlled_domains::GetDomainAndRegistry(gurl, filter));
}

// Return whether the given URL uses the Google.com domain.
// See google_util::IsGoogleDomainUrl for details.
static jboolean JNI_UrlUtilities_IsGoogleDomainUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& url,
    jboolean allow_non_standard_port) {
  GURL gurl = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleDomainUrl(
      gurl, google_util::DISALLOW_SUBDOMAIN,
      allow_non_standard_port == JNI_TRUE
          ? google_util::ALLOW_NON_STANDARD_PORTS
          : google_util::DISALLOW_NON_STANDARD_PORTS);
}

// Returns whether the given URL is a Google.com domain or sub-domain.
// See google_util::IsGoogleDomainUrl for details.
static jboolean JNI_UrlUtilities_IsGoogleSubDomainUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  GURL gurl = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleDomainUrl(
      gurl, google_util::ALLOW_SUBDOMAIN,
      google_util::DISALLOW_NON_STANDARD_PORTS);
}

// Returns whether the given URL is a Google.com Search URL.
// See google_util::IsGoogleSearchUrl for details.
static jboolean JNI_UrlUtilities_IsGoogleSearchUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  GURL gurl = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleSearchUrl(gurl);
}

// Returns whether the given URL is the Google Web Search URL.
// See google_util::IsGoogleHomePageUrl for details.
static jboolean JNI_UrlUtilities_IsGoogleHomePageUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  GURL gurl = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleHomePageUrl(gurl);
}

static jboolean JNI_UrlUtilities_IsUrlWithinScope(
    JNIEnv* env,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& scope_url) {
  GURL gurl = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url);
  GURL gscope_url = JNI_UrlUtilities_ConvertJavaStringToGURL(env, scope_url);
  return gurl.GetOrigin() == gscope_url.GetOrigin() &&
         base::StartsWith(gurl.path(), gscope_url.path(),
                          base::CompareCase::SENSITIVE);
}

// Returns whether the given URLs match, ignoring the fragment portions of the
// URLs.
static jboolean JNI_UrlUtilities_UrlsMatchIgnoringFragments(
    JNIEnv* env,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& url2) {
  GURL gurl = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url);
  GURL gurl2 = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url2);
  if (gurl.is_empty())
    return gurl2.is_empty();
  if (!gurl.is_valid() || !gurl2.is_valid())
    return false;

  GURL::Replacements replacements;
  replacements.SetRefStr("");
  return gurl.ReplaceComponents(replacements) ==
         gurl2.ReplaceComponents(replacements);
}

// Returns whether the given URLs have fragments that differ.
static jboolean JNI_UrlUtilities_UrlsFragmentsDiffer(
    JNIEnv* env,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& url2) {
  GURL gurl = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url);
  GURL gurl2 = JNI_UrlUtilities_ConvertJavaStringToGURL(env, url2);
  if (gurl.is_empty())
    return !gurl2.is_empty();
  if (!gurl.is_valid() || !gurl2.is_valid())
    return true;
  return gurl.ref() != gurl2.ref();
}

static jboolean JNI_UrlUtilities_IsAcceptedScheme(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return CheckSchemeBelongsToList(env, url, g_supported_schemes);
}

static jboolean JNI_UrlUtilities_IsValidForIntentFallbackNavigation(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return CheckSchemeBelongsToList(env, url, g_fallback_valid_schemes);
}

static jboolean JNI_UrlUtilities_IsDownloadable(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return CheckSchemeBelongsToList(env, url, g_downloadable_schemes);
}
