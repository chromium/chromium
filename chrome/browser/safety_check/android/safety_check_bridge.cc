// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_check/android/safety_check_bridge.h"

#include <jni.h>

#include <memory>

#include "chrome/browser/safety_check/android/jni_headers/SafetyCheckBridge_jni.h"
#include "chrome/browser/signin/identity_manager_provider.h"
#include "components/embedder_support/android/browser_context/browser_context_handle.h"
#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"
#include "components/safety_check/safety_check.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

static jlong JNI_SafetyCheckBridge_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_safety_check_observer) {
  return reinterpret_cast<intptr_t>(
      new SafetyCheckBridge(j_safety_check_observer));
}

static jboolean JNI_SafetyCheckBridge_UserSignedIn(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jhandle) {
  return password_manager::AuthenticatedLeakCheck::HasAccountForRequest(
      signin::GetIdentityManagerForBrowserContext(
          browser_context::BrowserContextFromJavaHandle(jhandle)));
}

SafetyCheckBridge::SafetyCheckBridge(
    const base::android::JavaParamRef<jobject>& j_safety_check_observer)
    : j_safety_check_observer_(j_safety_check_observer) {
  safety_check_ = std::make_unique<safety_check::SafetyCheck>(this);
}

void SafetyCheckBridge::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  safety_check_.reset();
  delete this;
}

void SafetyCheckBridge::CheckSafeBrowsing(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jobject>& jhandle) {
  safety_check_->CheckSafeBrowsing(user_prefs::UserPrefs::Get(
      browser_context::BrowserContextFromJavaHandle(jhandle)));
}

void SafetyCheckBridge::OnSafeBrowsingCheckResult(
    safety_check::SafetyCheck::SafeBrowsingStatus status) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SafetyCheckCommonObserver_onSafeBrowsingCheckResult(
      env, j_safety_check_observer_, static_cast<int>(status));
}

SafetyCheckBridge::~SafetyCheckBridge() = default;
