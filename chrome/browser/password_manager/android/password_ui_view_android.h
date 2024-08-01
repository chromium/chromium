// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_UI_VIEW_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_UI_VIEW_ANDROID_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_entry_edit/android/credential_edit_bridge.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class Profile;

namespace password_manager {
class CredentialProviderInterface;
}

// PasswordUIView for Android, contains jni hooks that allows Android UI to
// display passwords and route UI commands back to SavedPasswordsPresenter.
class PasswordUIViewAndroid
    : public password_manager::SavedPasswordsPresenter::Observer {
 public:
  // Result of transforming a vector of PasswordForms into their CSV
  // description and writing that to disk.
  struct SerializationResult {
    // The number of password entries written. 0 if error encountered.
    int entries_count;

    // The path to the temporary file containing the serialized passwords. Empty
    // if error encountered.
    std::string exported_file_path;

    // The error description recorded after the last write operation. Empty if
    // no error encountered.
    std::string error;
  };

  PasswordUIViewAndroid(JNIEnv* env,
                        const jni_zero::JavaRef<jobject>& obj,
                        Profile* profile);

  PasswordUIViewAndroid(const PasswordUIViewAndroid&) = delete;
  PasswordUIViewAndroid& operator=(const PasswordUIViewAndroid&) = delete;

  ~PasswordUIViewAndroid() override;

  // Calls from Java.
  base::android::ScopedJavaLocalRef<jobject> GetSavedPasswordEntry(
      JNIEnv* env,
      const base::android::JavaRef<jobject>&,
      int index);
  std::string GetSavedPasswordException(JNIEnv* env,
                                        const base::android::JavaRef<jobject>&,
                                        int index);
  void InsertPasswordEntryForTesting(JNIEnv* env,
                                     const std::u16string& origin,
                                     const std::u16string& username,
                                     const std::u16string& password);
  void UpdatePasswordLists(JNIEnv* env, const base::android::JavaRef<jobject>&);
  void HandleRemoveSavedPasswordEntry(JNIEnv* env,
                                      const base::android::JavaRef<jobject>&,
                                      int index);
  void HandleRemoveSavedPasswordException(
      JNIEnv* env,
      const base::android::JavaRef<jobject>&,
      int index);
  void HandleSerializePasswords(
      JNIEnv* env,
      const base::android::JavaRef<jobject>&,
      const std::string& java_target_directory,
      const base::android::JavaRef<jobject>& success_callback,
      const base::android::JavaRef<jobject>& error_callback);
  void HandleShowPasswordEntryEditingView(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& context,
      int index,
      const base::android::JavaParamRef<jobject>& obj);
  void HandleShowBlockedCredentialView(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& context,
      int index,
      const base::android::JavaParamRef<jobject>& obj);
  void ShowMigrationWarning(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& activity,
      const base::android::JavaParamRef<jobject>& controller);
  jboolean IsWaitingForPasswordStore(JNIEnv* env,
                                     const base::android::JavaRef<jobject>&);
  // Destroy the native implementation.
  void Destroy(JNIEnv*, const base::android::JavaRef<jobject>&);

  void OnEditUIDismissed();

  void set_export_target_for_testing(
      SerializationResult* export_target_for_testing) {
    export_target_for_testing_ = export_target_for_testing;
  }

  void set_credential_provider_for_testing(
      password_manager::CredentialProviderInterface* provider) {
    credential_provider_for_testing_ = provider;
  }

 private:
  // Possible states in the life of PasswordUIViewAndroid.
  // ALIVE:
  //   * Destroy was not called and no background tasks are pending.
  //   * All data members can be used on the main task runner.
  // ALIVE_SERIALIZATION_PENDING:
  //   * Destroy was not called, password serialization task on another task
  //     runner is running.
  //   * All data members can be used on the main task runner, except for
  //     |saved_passwords_presenter_| which can only be used inside
  //     ObtainAndSerializePasswords, which is being run on a backend task
  //     runner.
  // DELETION_PENDING:
  //   * Destroy() was called, a background task is pending and |this| should
  //     be deleted once the tasks complete.
  //   * This state should not be reached anywhere but in the completion call
  //     of the pending task.
  enum class State { ALIVE, ALIVE_SERIALIZATION_PENDING, DELETION_PENDING };

  // password_manager::SavedPasswordsPresenter::Observer implementation.
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  void UpdatePasswordLists();

  // Sends |serialization_result| to Java via |success_callback| or
  // |error_callback|, depending on whether the result is a success or an error.
  void PostSerializedPasswords(
      const base::android::JavaRef<jobject>& success_callback,
      const base::android::JavaRef<jobject>& error_callback,
      SerializationResult serialization_result);

  // The |state_| must only be accessed on the main task runner.
  State state_ = State::ALIVE;

  // If not null, PostSerializedPasswords will write the serialized passwords to
  // |*export_target_for_testing_| instead of passing them to Java. This must
  // remain null in production code.
  raw_ptr<SerializationResult> export_target_for_testing_ = nullptr;

  raw_ptr<Profile> profile_;

  // Pointer to the password store, powering |saved_passwords_presenter_|.
  scoped_refptr<password_manager::PasswordStoreInterface> profile_store_;

  // Manages the list of saved passwords, including updates.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_;

  // Cached passwords and blocked sites.
  std::vector<password_manager::CredentialUIEntry> passwords_;
  std::vector<password_manager::CredentialUIEntry> blocked_sites_;

  // If not null, passwords for exporting will be obtained from
  // |*credential_provider_for_testing_|, otherwise from
  // |saved_passwords_presenter_|. This must remain null in production code.
  raw_ptr<password_manager::CredentialProviderInterface>
      credential_provider_for_testing_ = nullptr;

  // Java side of UI controller.
  JavaObjectWeakGlobalRef weak_java_ui_controller_;

  // Used to open the view/edit/delete UI.
  std::unique_ptr<CredentialEditBridge> credential_edit_bridge_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_UI_VIEW_ANDROID_H_
