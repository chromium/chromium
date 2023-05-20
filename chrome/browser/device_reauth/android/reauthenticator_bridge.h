// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_REAUTHENTICATOR_BRIDGE_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_REAUTHENTICATOR_BRIDGE_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/device_reauth/device_authenticator.h"

// C++ counterpart of |ReauthenticatorBridge.java|. Used to mediate the
// biometric authentication requests.
class ReauthenticatorBridge {
 public:
  explicit ReauthenticatorBridge(
      const base::android::JavaParamRef<jobject>& java_bridge,
      jint requester);
  ~ReauthenticatorBridge();

  ReauthenticatorBridge(const ReauthenticatorBridge&) = delete;
  ReauthenticatorBridge& operator=(const ReauthenticatorBridge&) = delete;

  // Called by Java to check if biometric authentication can be used.
  bool CanUseAuthenticationWithBiometric(JNIEnv* env);

  // Called by Java to check if biometric or screen lock authentication can be
  // used.
  bool CanUseAuthenticationWithBiometricOrScreenLock(JNIEnv* env);

  // Called by Java to start authentication.
  void Reauthenticate(JNIEnv* env, bool use_last_valid_auth);

  // Called when reauthentication is completed.
  void OnReauthenticationCompleted(bool auth_succeeded);

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;

  // The authentication requester.
  device_reauth::DeviceAuthRequester requester_;

  // The authenticator used to trigger a biometric re-auth.
  scoped_refptr<device_reauth::DeviceAuthenticator> authenticator_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_REAUTHENTICATOR_BRIDGE_H_
