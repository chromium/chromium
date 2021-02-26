// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/origin_verifier.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "chrome/browser/android/browserservices/verification/jni_headers/OriginVerifier_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/digital_asset_links/digital_asset_links_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/simple_url_loader.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using base::android::JavaRef;
using digital_asset_links::RelationshipCheckResult;

namespace customtabs {

// static variables are zero-initialized.
int OriginVerifier::clear_browsing_data_call_count_for_tests_;

OriginVerifier::OriginVerifier(JNIEnv* env,
                               const JavaRef<jobject>& obj,
                               const JavaRef<jobject>& jweb_contents,
                               const JavaRef<jobject>& jprofile) {
  jobject_.Reset(obj);
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  url_loader_factory_ =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
  web_contents_ = content::WebContents::FromJavaWebContents(jweb_contents);
}

OriginVerifier::~OriginVerifier() = default;

bool OriginVerifier::VerifyOrigin(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jstring>& j_package_name,
                                  const JavaParamRef<jstring>& j_fingerprint,
                                  const JavaParamRef<jstring>& j_origin,
                                  const JavaParamRef<jstring>& j_relationship) {
  if (!j_package_name || !j_fingerprint || !j_origin || !j_relationship)
    return false;

  std::string package_name = ConvertJavaStringToUTF8(env, j_package_name);
  std::string fingerprint = ConvertJavaStringToUTF8(env, j_fingerprint);
  std::string origin = ConvertJavaStringToUTF8(env, j_origin);
  std::string relationship = ConvertJavaStringToUTF8(env, j_relationship);

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto asset_link_handler =
      std::make_unique<digital_asset_links::DigitalAssetLinksHandler>(
          url_loader_factory_, web_contents_);

  auto* asset_link_handler_ptr = asset_link_handler.get();

  return asset_link_handler_ptr->CheckDigitalAssetLinkRelationshipForAndroidApp(
      origin, relationship, fingerprint, package_name,
      base::BindOnce(&customtabs::OriginVerifier::OnRelationshipCheckComplete,
                     base::Unretained(this), std::move(asset_link_handler),
                     origin));
}

void OriginVerifier::OnRelationshipCheckComplete(
    std::unique_ptr<digital_asset_links::DigitalAssetLinksHandler> handler,
    const std::string& origin,
    RelationshipCheckResult result) {
  JNIEnv* env = base::android::AttachCurrentThread();

  auto j_origin = base::android::ConvertUTF8ToJavaString(env, origin);

  Java_OriginVerifier_onOriginVerificationResult(env, jobject_, j_origin,
                                                 static_cast<jint>(result));
}

void OriginVerifier::Destroy(JNIEnv* env,
                             const base::android::JavaRef<jobject>& obj) {
  delete this;
}

// static
void OriginVerifier::ClearBrowsingData() {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_OriginVerifier_clearBrowsingData(env);
  clear_browsing_data_call_count_for_tests_++;
}

// static
int OriginVerifier::GetClearBrowsingDataCallCountForTesting() {
  return clear_browsing_data_call_count_for_tests_;
}

static jlong JNI_OriginVerifier_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobject>& jprofile) {
  if (!g_browser_process)
    return 0;

  OriginVerifier* native_verifier =
      new OriginVerifier(env, obj, jweb_contents, jprofile);
  return reinterpret_cast<intptr_t>(native_verifier);
}

}  // namespace customtabs
