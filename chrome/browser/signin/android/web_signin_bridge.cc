// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android//web_signin_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/signin/services/android/jni_headers/WebSigninBridge_jni.h"

using base::android::JavaParamRef;

namespace {
void ForwardOnSigninCompletedToJava(
    const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
    signin::WebSigninTracker::Result result) {
  Java_WebSigninBridge_onSigninResult(base::android::AttachCurrentThread(),
                                      j_callback, static_cast<jint>(result));
}
}  // namespace

WebSigninBridge::WebSigninBridge(
    signin::IdentityManager* identity_manager,
    AccountReconcilor* account_reconcilor,
    CoreAccountId signin_account,
    base::OnceCallback<void(signin::WebSigninTracker::Result)> callback)
    : tracker_(identity_manager,
               account_reconcilor,
               std::move(signin_account),
               std::move(callback)) {}

WebSigninBridge::~WebSigninBridge() = default;

static jlong JNI_WebSigninBridge_Create(
    JNIEnv* env,
    Profile* profile,
    CoreAccountInfo& account,
    const JavaParamRef<jobject>& j_listener) {
  DCHECK(j_listener) << "Listener should be non-null";

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  base::OnceCallback<void(signin::WebSigninTracker::Result)>
      on_signin_completed = base::BindOnce(
          &ForwardOnSigninCompletedToJava,
          base::android::ScopedJavaGlobalRef<jobject>(j_listener));
  return reinterpret_cast<intptr_t>(
      new WebSigninBridge(identity_manager, account_reconcilor,
                          account.account_id, std::move(on_signin_completed)));
}

static void JNI_WebSigninBridge_Destroy(JNIEnv* env, jlong web_signin_bridge) {
  delete reinterpret_cast<WebSigninBridge*>(web_signin_bridge);
}
