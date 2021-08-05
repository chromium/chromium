// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_COOKIE_CONTROLS_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_COOKIE_CONTROLS_SERVICE_BRIDGE_H_

#include "base/android/jni_weak_ref.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"

// Communicates between CookieControlsService (C++ backend) and observers
// in the Incognito NTP Java UI.
class CookieControlsServiceBridge : public CookieControlsService::Observer {
 public:
  CookieControlsServiceBridge(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  ~CookieControlsServiceBridge() override;

  // Called by the Java counterpart when it is getting garbage collected.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void HandleCookieControlsToggleChanged(JNIEnv* env, jboolean checked);

  void UpdateServiceIfNecessary(JNIEnv* env);

  // CookieControlsService::Observer
  void OnThirdPartyCookieBlockingPrefChanged() override;
  void OnThirdPartyCookieBlockingPolicyChanged() override;

 private:
  // Updates cookie controls UI when third-party cookie blocking setting has
  // changed.
  void SendCookieControlsUIChanges();
  // Starts a service to observe the current profile.
  void UpdateServiceIfNecessary();

  CookieControlsService* service_;
  base::android::ScopedJavaGlobalRef<jobject> jobject_;

  DISALLOW_COPY_AND_ASSIGN(CookieControlsServiceBridge);
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_COOKIE_CONTROLS_SERVICE_BRIDGE_H_
