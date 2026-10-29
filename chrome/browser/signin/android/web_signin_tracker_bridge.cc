// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android/web_signin_tracker_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "google_apis/gaia/core_account_id.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/signin/services/android/jni_headers/WebSigninTrackerBridge_jni.h"

using base::android::JavaRef;

namespace {
void ForwardOnSigninCompletedToJava(
    const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
    signin::WebSigninTracker::Result result) {
  Java_WebSigninTrackerBridge_onSigninResult(
      base::android::AttachCurrentThread(), j_callback,
      static_cast<int32_t>(result));
}
}  // namespace

WebSigninTrackerBridge::WebSigninTrackerBridge(
    signin::IdentityManager* identity_manager,
    AccountReconcilor* account_reconcilor,
    std::variant<CoreAccountId, std::string> signin_account,
    base::OnceCallback<void(signin::WebSigninTracker::Result)> callback)
    : tracker_(identity_manager,
               account_reconcilor,
               std::move(signin_account),
               std::move(callback)) {}

WebSigninTrackerBridge::~WebSigninTrackerBridge() = default;

void WebSigninTrackerBridge::Start() {
  // May destroy `this`; don't access members afterwards.
  tracker_.Start();
}

static int64_t JNI_WebSigninTrackerBridge_CreateWithCoreAccountId(
    JNIEnv* env,
    Profile* profile,
    const CoreAccountId& account_id,
    const JavaRef<jobject>& j_listener) {
  CHECK(j_listener) << "Listener should be non-null";

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  base::OnceCallback<void(signin::WebSigninTracker::Result)>
      on_signin_completed = base::BindOnce(
          &ForwardOnSigninCompletedToJava,
          base::android::ScopedJavaGlobalRef<jobject>(j_listener));
  return reinterpret_cast<intptr_t>(
      new WebSigninTrackerBridge(identity_manager, account_reconcilor,
                                 account_id, std::move(on_signin_completed)));
}

static int64_t JNI_WebSigninTrackerBridge_CreateWithEmail(
    JNIEnv* env,
    Profile* profile,
    const std::string& account_email,
    const JavaRef<jobject>& j_listener) {
  CHECK(j_listener) << "Listener should be non-null";

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  base::OnceCallback<void(signin::WebSigninTracker::Result)>
      on_signin_completed = base::BindOnce(
          &ForwardOnSigninCompletedToJava,
          base::android::ScopedJavaGlobalRef<jobject>(j_listener));
  return reinterpret_cast<intptr_t>(new WebSigninTrackerBridge(
      identity_manager, account_reconcilor, account_email,
      std::move(on_signin_completed)));
}

static void JNI_WebSigninTrackerBridge_Start(
    JNIEnv* env,
    int64_t web_signin_tracker_bridge) {
  // May destroy the bridge if the result is already known.
  reinterpret_cast<WebSigninTrackerBridge*>(web_signin_tracker_bridge)->Start();
}

static void JNI_WebSigninTrackerBridge_Destroy(
    JNIEnv* env,
    int64_t web_signin_tracker_bridge) {
  delete reinterpret_cast<WebSigninTrackerBridge*>(web_signin_tracker_bridge);
}

DEFINE_JNI(WebSigninTrackerBridge)
