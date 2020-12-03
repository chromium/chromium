// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/continuous_search/internal/search_url_helper.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/continuous_search/internal/jni_headers/SearchUrlHelper_jni.h"
#include "components/google/core/common/google_util.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_util.h"

namespace continuous_search {

base::android::ScopedJavaLocalRef<jstring> JNI_SearchUrlHelper_GetQueryIfSrpUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_gurl) {
  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_gurl);
  if (!url->is_valid())
    return base::android::ScopedJavaLocalRef<jstring>();

  base::Optional<std::string> query = ExtractSearchQueryIfGoogle(*url);

  return query.has_value()
             ? base::android::ConvertUTF8ToJavaString(env, query.value())
             : base::android::ScopedJavaLocalRef<jstring>();
}

base::Optional<std::string> ExtractSearchQueryIfGoogle(const GURL& url) {
  if (!google_util::IsGoogleSearchUrl(url))
    return base::nullopt;

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
  return base::nullopt;
}

}  // namespace continuous_search
