// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_ENTRY_EDIT_ANDROID_CREDENTIAL_EDIT_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_ENTRY_EDIT_ANDROID_CREDENTIAL_EDIT_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

// This bridge is responsible for creating and releasing its Java counterpart,
// in order to launch or dismiss the edit UI.
class CredentialEditBridge {
 public:
  CredentialEditBridge();
  ~CredentialEditBridge();

  CredentialEditBridge(const CredentialEditBridge&) = delete;
  CredentialEditBridge& operator=(const CredentialEditBridge&) = delete;

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

#endif  // CHROME_BROWSER_PASSWORD_ENTRY_EDIT_ANDROID_CREDENTIAL_EDIT_BRIDGE_H_
