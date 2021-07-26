// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/continuous_search/internal/search_url_helper.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/continuous_search/internal/jni_headers/SearchUrlHelper_jni.h"
#include "chrome/browser/continuous_search/page_category.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_util.h"

namespace continuous_search {

jboolean JNI_SearchUrlHelper_IsGoogleDomainUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl) {
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_gurl);
  if (!url->is_valid())
    return false;
  return static_cast<jboolean>(
      google_util::IsGoogleDomainUrl(*url, google_util::DISALLOW_SUBDOMAIN,
                                     google_util::DISALLOW_NON_STANDARD_PORTS));
}

base::android::ScopedJavaLocalRef<jstring>
JNI_SearchUrlHelper_GetQueryIfValidSrpUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl) {
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_gurl);
  if (!url->is_valid())
    return base::android::ScopedJavaLocalRef<jstring>();

  absl::optional<std::string> query = ExtractSearchQueryIfValidUrl(*url);

  return query.has_value()
             ? base::android::ConvertUTF8ToJavaString(env, query.value())
             : base::android::ScopedJavaLocalRef<jstring>();
}

jint JNI_SearchUrlHelper_GetSrpPageCategoryFromUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl) {
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_gurl);
  if (!url->is_valid())
    return static_cast<jint>(PageCategory::kNone);

  return static_cast<jint>(GetSrpPageCategoryForUrl(*url));
}

absl::optional<std::string> ExtractSearchQueryIfValidUrl(const GURL& url) {
  if (!google_util::IsGoogleSearchUrl(url) ||
      GetSrpPageCategoryForUrl(url) == PageCategory::kNone)
    return absl::nullopt;

  base::StringPiece query_str = url.query_piece();
  url::Component query(0, static_cast<int>(query_str.length())), key, value;
  while (url::ExtractQueryKeyValue(query_str.data(), &query, &key, &value)) {
    base::StringPiece key_str = query_str.substr(key.begin, key.len);
    if (key_str == "q") {
      base::StringPiece value_str = query_str.substr(value.begin, value.len);
      return base::UnescapeURLComponent(
          value_str,
          base::UnescapeRule::REPLACE_PLUS_WITH_SPACE |
              base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
    }
  }
  return absl::nullopt;
}

PageCategory GetSrpPageCategoryForUrl(const GURL& url) {
  std::string value;
  // Organic results tab
  if (!net::GetValueForKeyInQuery(url, "tbm", &value))
    return PageCategory::kOrganicSrp;
  // News tab
  if (value == "nws")
    return PageCategory::kNewsSrp;
  return PageCategory::kNone;
}

GURL GetOriginalUrlFromWebContents(content::WebContents* web_contents) {
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();

  if (!entry || entry->GetRedirectChain().size() <= 1) {
    return GURL();
  }
  return entry->GetRedirectChain().front();
}

base::android::ScopedJavaLocalRef<jobject>
JNI_SearchUrlHelper_GetOriginalUrlFromWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  return url::GURLAndroid::FromNativeGURL(
      env, GetOriginalUrlFromWebContents(
               content::WebContents::FromJavaWebContents(j_web_contents)));
}

}  // namespace continuous_search
