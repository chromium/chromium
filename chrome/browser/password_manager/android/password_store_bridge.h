// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class Profile;

class PasswordStoreBridge
    : public password_manager::SavedPasswordsPresenter::Observer {
 public:
  PasswordStoreBridge(const base::android::JavaParamRef<jobject>& java_bridge,
                      Profile* profile);
  ~PasswordStoreBridge() override;

  PasswordStoreBridge(const PasswordStoreBridge&) = delete;
  PasswordStoreBridge& operator=(const PasswordStoreBridge&) = delete;

  // Called by Java to store a new credential into the profile password store.
  void InsertPasswordCredentialInProfileStoreForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& credential);

  // Called by Java to store a new credential into the account password store.
  void InsertPasswordCredentialInAccountStoreForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& credential);

  void BlocklistForTesting(JNIEnv* env,
                           const base::android::JavaParamRef<jstring>& jurl);

  // Called by Java to edit a credential.
  bool EditPassword(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& credential,
                    const base::android::JavaParamRef<jstring>& new_password);

  // Called by Java to get the number of stored credentials for both profile and
  // account stores.
  jint GetPasswordStoreCredentialsCountForAllStores(JNIEnv* env) const;

  // Called by Java to get the number of stored credentials in the account
  // storage.
  jint GetPasswordStoreCredentialsCountForAccountStore(JNIEnv* env) const;

  // Called by Java to get the number of stored credentials in the local
  // storage.
  jint GetPasswordStoreCredentialsCountForProfileStore(JNIEnv* env) const;

  // Called by Java to get all stored credentials.
  void GetAllCredentials(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& java_credentials) const;

  // Called by Java to clear all stored passwords.
  void ClearAllPasswords(JNIEnv* env);

  // Called by Java to clear all passwords from profile store.
  void ClearAllPasswordsFromProfileStore(JNIEnv* env);

  // Called by Java to destroy `this`.
  void Destroy(JNIEnv* env);

 private:
  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  void OnEdited(const password_manager::CredentialUIEntry& form) override;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;

  raw_ptr<Profile> profile_;

  const scoped_refptr<password_manager::PasswordStoreInterface> profile_store_;
  const scoped_refptr<password_manager::PasswordStoreInterface> account_store_;

  // Used to fetch and edit passwords.
  // TODO(crbug.com/40267119): Use PasswordStore directly.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_;

  // A scoped observer for `saved_passwords_presenter_`.
  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_password_presenter_{this};

  // `weak_factory_` is used for all callback uses.
  base::WeakPtrFactory<PasswordStoreBridge> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_BRIDGE_H_
