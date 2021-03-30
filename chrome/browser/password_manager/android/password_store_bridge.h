// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class PasswordStoreBridge
    : public password_manager::SavedPasswordsPresenter::Observer {
 public:
  explicit PasswordStoreBridge(
      const base::android::JavaParamRef<jobject>& java_bridge);
  ~PasswordStoreBridge() override;

  PasswordStoreBridge(const PasswordStoreBridge&) = delete;
  PasswordStoreBridge& operator=(const PasswordStoreBridge&) = delete;

  // Called by Java to store a new credential into the password store.
  void InsertPasswordCredential(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& credential);

  // Called by Java to edit a credential.
  bool EditPassword(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& credential,
                    const base::android::JavaParamRef<jstring>& new_password);

  // Called by Java to get the number of stored credentials.
  jint GetPasswordStoreCredentialsCount(JNIEnv* env) const;

  // Called by Java to get all stored credentials.
  void GetAllCredentials(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& java_credentials) const;

  // Called by Java to clear all stored passwords.
  void ClearAllPasswords(JNIEnv* env);

  // Called by Java to destroy `this`.
  void Destroy(JNIEnv* env);

 private:
  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      password_manager::SavedPasswordsPresenter::SavedPasswordsView passwords)
      override;

  void OnEdited(const password_manager::PasswordForm& form) override;

  // Callback executed after clearing the password store. It re-initializes
  // `saved_passwords_presenter_`.
  void OnPasswordStoreCleared(bool success);

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;

  // Handle to the password store, powering `saved_passwords_presenter_`.
  scoped_refptr<password_manager::PasswordStore> password_store_ =
      PasswordStoreFactory::GetForProfile(ProfileManager::GetLastUsedProfile(),
                                          ServiceAccessType::EXPLICIT_ACCESS);

  // Used to fetch and edit passwords.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_{
      password_store_};

  // A scoped observer for `saved_passwords_presenter_`.
  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_password_presenter_{this};

  // `weak_factory_` is used for all callback uses.
  base::WeakPtrFactory<PasswordStoreBridge> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_BRIDGE_H_
