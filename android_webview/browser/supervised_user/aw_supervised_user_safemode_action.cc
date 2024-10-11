// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/supervised_user/aw_supervised_user_safemode_action.h"

#include "android_webview/browser/supervised_user/aw_supervised_user_url_classifier.h"
#include "base/check_op.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwSupervisedUserSafeModeAction_jni.h"

namespace android_webview {

// static
AwSupervisedUserSafeModeAction* AwSupervisedUserSafeModeAction::GetInstance() {
  static base::NoDestructor<AwSupervisedUserSafeModeAction> instance;
  return instance.get();
}

void AwSupervisedUserSafeModeAction::SetSupervisionEnabled(bool value) {
  // This uses locking instead of thread assertions because SafeMode is executed
  // before the Chromium UI thread is configured.
  base::AutoLock lock(lock_);
  is_supervision_enabled_ = value;
}

bool AwSupervisedUserSafeModeAction::IsSupervisionEnabled() {
  // This uses locking instead of thread assertions because SafeMode is executed
  // before the Chromium UI thread is configured.
  base::AutoLock lock(lock_);
  return is_supervision_enabled_;
}

static void JNI_AwSupervisedUserSafeModeAction_SetSupervisionEnabled(
    JNIEnv* env,
    jboolean value) {
  AwSupervisedUserSafeModeAction::GetInstance()->SetSupervisionEnabled(value);
}

static jboolean JNI_AwSupervisedUserSafeModeAction_IsSupervisionEnabled(
    JNIEnv* env) {
  return AwSupervisedUserSafeModeAction::GetInstance()->IsSupervisionEnabled();
}

}  // namespace android_webview
