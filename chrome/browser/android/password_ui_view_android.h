// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_UI_VIEW_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_UI_VIEW_ANDROID_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {
class CredentialProviderInterface;
}

// PasswordUIView for Android, contains jni hooks that allows Android UI to
// display passwords and route UI commands back to native
// PasswordManagerPresenter.
class PasswordUIViewAndroid : public PasswordUIView {
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

  PasswordUIViewAndroid(JNIEnv* env, jobject);
  ~PasswordUIViewAndroid() override;

  // PasswordUIView implementation.
  Profile* GetProfile() override;
  void SetPasswordList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list)
      override;
  void SetPasswordExceptionList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&
          password_exception_list) override;

  // Calls from Java.
  base::android::ScopedJavaLocalRef<jobject> GetSavedPasswordEntry(
      JNIEnv* env,
      const base::android::JavaRef<jobject>&,
      int index);
  base::android::ScopedJavaLocalRef<jstring> GetSavedPasswordException(
      JNIEnv* env,
      const base::android::JavaRef<jobject>&,
      int index);
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
      const base::android::JavaRef<jstring>& java_target_directory,
      const base::android::JavaRef<jobject>& success_callback,
      const base::android::JavaRef<jobject>& error_callback);
  void HandleShowPasswordEntryEditingView(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& context,
      int index);
  // Destroy the native implementation.
  void Destroy(JNIEnv*, const base::android::JavaRef<jobject>&);

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
  //     |password_manager_presenter_| which can only be used inside
  //     ObtainAndSerializePasswords, which is being run on a backend task
  //     runner.
  // DELETION_PENDING:
  //   * Destroy() was called, a background task is pending and |this| should
  //     be deleted once the tasks complete.
  //   * This state should not be reached anywhere but in the compeltion call
  //     of the pending task.
  enum class State { ALIVE, ALIVE_SERIALIZATION_PENDING, DELETION_PENDING };

  // Calls |password_manager_presenter_| to retrieve cached PasswordForm
  // objects, then PasswordCSVWriter to serialize them, and finally writes them
  // to a temporary file in |target_directory|. The steps involve a lot of
  // memory allocation and copying, as well as I/O operations, so this method
  // should be executed on a suitable task runner.
  SerializationResult ObtainAndSerializePasswords(
      const base::FilePath& target_directory);

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
  SerializationResult* export_target_for_testing_ = nullptr;

  PasswordManagerPresenter password_manager_presenter_;

  // If not null, passwords for exporting will be obtained from
  // |*credential_provider_for_testing_|, otherwise from
  // |password_manager_presenter_|. This must remain null in production code.
  password_manager::CredentialProviderInterface*
      credential_provider_for_testing_ = nullptr;

  // Java side of UI controller.
  JavaObjectWeakGlobalRef weak_java_ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordUIViewAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_UI_VIEW_ANDROID_H_
