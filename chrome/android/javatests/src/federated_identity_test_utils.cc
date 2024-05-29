// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_test_util_jni/FederatedIdentityTestUtils_jni.h"

namespace federated_identity {

void JNI_FederatedIdentityTestUtils_EmbargoFedCmForRelyingParty(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url) {
  url::Origin origin =
      url::Origin::Create(url::GURLAndroid::ToNativeGURL(env, j_url));
  Profile* profile = ProfileManager::GetLastUsedProfile();
  profile->GetFederatedIdentityApiPermissionContext()->RecordDismissAndEmbargo(
      origin);
}

}  // namespace federated_identity
