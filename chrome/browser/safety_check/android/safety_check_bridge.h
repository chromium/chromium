// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFETY_CHECK_ANDROID_SAFETY_CHECK_BRIDGE_H_
#define CHROME_BROWSER_SAFETY_CHECK_ANDROID_SAFETY_CHECK_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "components/prefs/pref_service.h"
#include "components/safety_check/safety_check.h"

// Allows the Java code to make use of cross-platform browser safety checks in
// //components/safety_check.
class SafetyCheckBridge
    : public safety_check::SafetyCheck::SafetyCheckHandlerInterface {
 public:
  // Takes an observer object that will get invoked on check results.
  SafetyCheckBridge(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_safety_check_observer);

  // Destroys this bridge. Should only be invoked by the Java side.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Checks the status of Safe Browsing and invokes |OnSafeBrowsingCheckResult|
  // on the observer object with the result.
  void CheckSafeBrowsing(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  // safety_check::SafetyCheck::SafetyCheckHandlerInterface implementation.
  void OnSafeBrowsingCheckResult(
      safety_check::SafetyCheck::SafeBrowsingStatus status) override;

 private:
  virtual ~SafetyCheckBridge();

  PrefService* pref_service_ = nullptr;
  std::unique_ptr<safety_check::SafetyCheck> safety_check_;
  base::android::ScopedJavaGlobalRef<jobject> j_safety_check_observer_;
};

#endif  // CHROME_BROWSER_SAFETY_CHECK_ANDROID_SAFETY_CHECK_BRIDGE_H_
