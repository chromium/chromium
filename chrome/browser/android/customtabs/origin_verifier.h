// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CUSTOMTABS_ORIGIN_VERIFIER_H_
#define CHROME_BROWSER_ANDROID_CUSTOMTABS_ORIGIN_VERIFIER_H_

#include "base/android/scoped_java_ref.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

namespace digital_asset_links {
enum class RelationshipCheckResult;
class DigitalAssetLinksHandler;
}  // namespace digital_asset_links

namespace customtabs {

// JNI bridge for OriginVerifier.java
class OriginVerifier {
 public:
  OriginVerifier(JNIEnv* env,
                 const base::android::JavaRef<jobject>& obj,
                 const base::android::JavaRef<jobject>& jweb_contents,
                 const base::android::JavaRef<jobject>& jprofile);
  ~OriginVerifier();

  // Verify origin with the given parameters. No network requests can be made
  // if the params are null.
  bool VerifyOrigin(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& j_package_name,
                    const base::android::JavaParamRef<jstring>& j_fingerprint,
                    const base::android::JavaParamRef<jstring>& j_origin,
                    const base::android::JavaParamRef<jstring>& j_relationship);

  void Destroy(JNIEnv* env, const base::android::JavaRef<jobject>& obj);

  static void ClearBrowsingData();
  static int GetClearBrowsingDataCallCountForTesting();
 private:
  void OnRelationshipCheckComplete(
      digital_asset_links::RelationshipCheckResult result);

  std::unique_ptr<digital_asset_links::DigitalAssetLinksHandler>
      asset_link_handler_;

  base::android::ScopedJavaGlobalRef<jobject> jobject_;

  static int clear_browsing_data_call_count_for_tests_;

  DISALLOW_COPY_AND_ASSIGN(OriginVerifier);
};

}  // namespace customtabs

#endif  // CHROME_BROWSER_ANDROID_CUSTOMTABS_ORIGIN_VERIFIER_H_
