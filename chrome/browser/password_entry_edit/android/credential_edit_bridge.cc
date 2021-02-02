// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_entry_edit/android/credential_edit_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/browser/password_entry_edit/android/jni_headers/CredentialEditBridge_jni.h"

CredentialEditBridge::CredentialEditBridge() {
  java_bridge_.Reset(Java_CredentialEditBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));
}

CredentialEditBridge::~CredentialEditBridge() {
  Java_CredentialEditBridge_destroy(base::android::AttachCurrentThread(),
                                    java_bridge_);
}
