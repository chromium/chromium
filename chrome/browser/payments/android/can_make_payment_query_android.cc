// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/CanMakePaymentQuery_jni.h"
#include "components/payments/content/can_make_payment_query_factory.h"
#include "components/payments/core/can_make_payment_query.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace payments {

// static
jboolean JNI_CanMakePaymentQuery_CanQuery(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jstring>& jtop_level_origin,
    const base::android::JavaParamRef<jstring>& jframe_origin,
    const base::android::JavaParamRef<jobject>& jquery_map,
    jboolean per_method_quota) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return false;

  std::vector<std::string> method_identifiers;
  base::android::AppendJavaStringArrayToStringVector(
      env, Java_CanMakePaymentQuery_getMethodIdentifiers(env, jquery_map),
      &method_identifiers);

  std::map<std::string, std::set<std::string>> query;
  for (const auto& method_identifier : method_identifiers) {
    std::set<std::string> method_specific_parameters = {
        base::android::ConvertJavaStringToUTF8(
            Java_CanMakePaymentQuery_getStringifiedMethodData(
                env, jquery_map,
                base::android::ConvertUTF8ToJavaString(env,
                                                       method_identifier)))};
    query.insert(std::make_pair(method_identifier, method_specific_parameters));
  }

  return CanMakePaymentQueryFactory::GetInstance()
      ->GetForContext(web_contents->GetBrowserContext())
      ->CanQuery(
          GURL(base::android::ConvertJavaStringToUTF8(env, jtop_level_origin)),
          GURL(base::android::ConvertJavaStringToUTF8(env, jframe_origin)),
          query, per_method_quota);
}

}  // namespace payments
