// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_ENTRY_EDIT_ANDROID_CREDENTIAL_EDIT_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_ENTRY_EDIT_ANDROID_CREDENTIAL_EDIT_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

// This bridge is responsible for creating and releasing its Java counterpart,
// in order to launch or dismiss the edit UI.
class CredentialEditBridge {
 public:
  using IsInsecureCredential =
      base::StrongAlias<class IsInsecureCredentialTag, bool>;
  // Returns a new bridge if none exists. If a bridge already exitst, it returns
  // null, since that means the edit UI is already open and it should not be
  // shared.
  static std::unique_ptr<CredentialEditBridge> MaybeCreate(
      const password_manager::CredentialUIEntry credential,
      IsInsecureCredential is_insecure_credential,
      std::vector<std::u16string> existing_usernames,
      password_manager::SavedPasswordsPresenter* saved_passwords_presenter,
      base::OnceClosure dismissal_callback,
      const base::android::JavaRef<jobject>& context);
  ~CredentialEditBridge();

  CredentialEditBridge(const CredentialEditBridge&) = delete;
  CredentialEditBridge& operator=(const CredentialEditBridge&) = delete;

  // Called by Java to get the credential to be edited.
  void GetCredential(JNIEnv* env);

  // Called by Java to get the existing usernames.
  void GetExistingUsernames(JNIEnv* env);

  // Called by Java to save the changes to the edited credential.
  void SaveChanges(JNIEnv* env,
                   const base::android::JavaParamRef<jstring>& username,
                   const base::android::JavaParamRef<jstring>& password);

  // Called by Java to remove the credential from the store.
  void DeleteCredential(JNIEnv* env);

  // Called by Java to signal that the UI was dismissed.
  void OnUIDismissed(JNIEnv* env);

 private:
  CredentialEditBridge(
      const password_manager::CredentialUIEntry credential,
      IsInsecureCredential is_insecure_credential,
      std::vector<std::u16string> existing_usernames,
      password_manager::SavedPasswordsPresenter* saved_passwords_presenter,
      base::OnceClosure dismissal_callback,
      const base::android::JavaRef<jobject>& context,
      base::android::ScopedJavaGlobalRef<jobject> java_bridge);

  // Returns the URL or app for which the credential was saved, formatted
  // for display.
  std::u16string GetDisplayURLOrAppName();

  // If the credential to be edited is a federated credential, it returns
  // the identity provider formatted for display. Otherwise, it returns an empty
  // string.
  std::u16string GetDisplayFederationOrigin();

  // The credential to be edited.
  const password_manager::CredentialUIEntry credential_;

  // Whether the credential being edited is an insecure credential. Used to
  // customize the deletion confirmation dialog string.
  IsInsecureCredential is_insecure_credential_;

  // All the usernames saved for the current site/app.
  std::vector<std::u16string> existing_usernames_;

  // The backend to route the edit event to. Should be owned by the the owner of
  // the bridge.
  raw_ptr<password_manager::SavedPasswordsPresenter>
      saved_passwords_presenter_ = nullptr;

  // Callback invoked when the UI is being dismissed from the Java side.
  base::OnceClosure dismissal_callback_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

#endif  // CHROME_BROWSER_PASSWORD_ENTRY_EDIT_ANDROID_CREDENTIAL_EDIT_BRIDGE_H_
