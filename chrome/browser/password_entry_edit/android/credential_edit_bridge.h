// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_ENTRY_EDIT_ANDROID_CREDENTIAL_EDIT_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_ENTRY_EDIT_ANDROID_CREDENTIAL_EDIT_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

// This bridge is responsible for creating and releasing its Java counterpart,
// in order to launch or dismiss the edit UI.
class CredentialEditBridge {
 public:
  // Returns a new bridge if none exists. If a bridge already exitst, it returns
  // null, since that means the edit UI is already open and it should not be
  // shared.
  static std::unique_ptr<CredentialEditBridge> MaybeCreate(
      const password_manager::PasswordForm* credential,
      base::OnceClosure dismissal_callback,
      const base::android::JavaRef<jobject>& context,
      const base::android::JavaRef<jobject>& settings_launcher);
  ~CredentialEditBridge();

  CredentialEditBridge(const CredentialEditBridge&) = delete;
  CredentialEditBridge& operator=(const CredentialEditBridge&) = delete;

  // Called by Java to get the credential to be edited.
  void GetCredential(JNIEnv* env);

  // Called by Java to signal that the UI was dismissed.
  void OnUIDismissed(JNIEnv* env);

 private:
  CredentialEditBridge(const password_manager::PasswordForm* credential,
                       base::OnceClosure dismissal_callback,
                       const base::android::JavaRef<jobject>& context,
                       const base::android::JavaRef<jobject>& settings_launcher,
                       base::android::ScopedJavaGlobalRef<jobject> java_bridge);

  // Returns the URL or app for which the credential was saved, formatted
  // for display.
  base::string16 GetDisplayURLOrAppName();

  // If the credential to be edited is a federated credential, it returns
  // the identity provider formatted for display. Otherwise, it returns an empty
  // string.
  base::string16 GetDisplayFederationOrigin();

  // The credential to be edited.
  const password_manager::PasswordForm* credential_ = nullptr;

  // Callback invoked when the UI is being dismissed from the Java side.
  base::OnceClosure dismissal_callback_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

#endif  // CHROME_BROWSER_PASSWORD_ENTRY_EDIT_ANDROID_CREDENTIAL_EDIT_BRIDGE_H_
