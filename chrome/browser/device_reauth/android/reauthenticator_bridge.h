// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_REAUTHENTICATOR_BRIDGE_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_REAUTHENTICATOR_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/device_reauth/device_authenticator.h"

class Profile;

// C++ counterpart of |ReauthenticatorBridge.java|. Used to mediate the
// biometric authentication requests.
class ReauthenticatorBridge {
 public:
  ReauthenticatorBridge(const base::android::JavaParamRef<jobject>& java_bridge,
                        const base::android::JavaParamRef<jobject>& activity,
                        Profile* profile,
                        jint requester);
  ~ReauthenticatorBridge();

  ReauthenticatorBridge(const ReauthenticatorBridge&) = delete;
  ReauthenticatorBridge& operator=(const ReauthenticatorBridge&) = delete;

  // Called by Java to check biometric availability status.
  jint GetBiometricAvailabilityStatus(JNIEnv* env);

  // Called by Java to start authentication.
  void Reauthenticate(JNIEnv* env);

  // Called when reauthentication is completed.
  void OnReauthenticationCompleted(bool auth_succeeded);

  // Called from java to delete this object.
  void Destroy(JNIEnv* env);

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;

  raw_ptr<Profile> profile_;

  // The authenticator used to trigger a biometric re-auth.
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_REAUTHENTICATOR_BRIDGE_H_
