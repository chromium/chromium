// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_ui_view_android.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/int_string_callback.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/android/chrome_jni_headers/PasswordUIView_jni.h"
#include "chrome/browser/android/password_editing_bridge.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

PasswordUIViewAndroid::PasswordUIViewAndroid(JNIEnv* env, jobject obj)
    : password_manager_presenter_(this), weak_java_ui_controller_(env, obj) {
  password_manager_presenter_.Initialize();
}

PasswordUIViewAndroid::~PasswordUIViewAndroid() {}

void PasswordUIViewAndroid::Destroy(JNIEnv*, const JavaRef<jobject>&) {
  switch (state_) {
    case State::ALIVE:
      delete this;
      break;
    case State::ALIVE_SERIALIZATION_PENDING:
      // Postpone the deletion until the pending tasks are completed, so that
      // the tasks do not attempt a use after free while reading data from
      // |this|.
      state_ = State::DELETION_PENDING;
      break;
    case State::DELETION_PENDING:
      NOTREACHED();
      break;
  }
}

Profile* PasswordUIViewAndroid::GetProfile() {
  return ProfileManager::GetLastUsedProfile();
}

void PasswordUIViewAndroid::SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> ui_controller = weak_java_ui_controller_.get(env);
  if (!ui_controller.is_null()) {
    Java_PasswordUIView_passwordListAvailable(
        env, ui_controller, static_cast<int>(password_list.size()));
  }
}

void PasswordUIViewAndroid::SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
        password_exception_list) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> ui_controller = weak_java_ui_controller_.get(env);
  if (!ui_controller.is_null()) {
    Java_PasswordUIView_passwordExceptionListAvailable(
        env, ui_controller, static_cast<int>(password_exception_list.size()));
  }
}

void PasswordUIViewAndroid::UpdatePasswordLists(JNIEnv* env,
                                                const JavaRef<jobject>&) {
  DCHECK_EQ(State::ALIVE, state_);
  password_manager_presenter_.UpdatePasswordLists();
}

ScopedJavaLocalRef<jobject> PasswordUIViewAndroid::GetSavedPasswordEntry(
    JNIEnv* env,
    const JavaRef<jobject>&,
    int index) {
  DCHECK_EQ(State::ALIVE, state_);
  const autofill::PasswordForm* form =
      password_manager_presenter_.GetPassword(index);
  if (!form) {
    return Java_PasswordUIView_createSavedPasswordEntry(
        env, ConvertUTF8ToJavaString(env, std::string()),
        ConvertUTF16ToJavaString(env, base::string16()),
        ConvertUTF16ToJavaString(env, base::string16()));
  }
  return Java_PasswordUIView_createSavedPasswordEntry(
      env,
      ConvertUTF8ToJavaString(
          env, password_manager::GetShownOriginAndLinkUrl(*form).first),
      ConvertUTF16ToJavaString(env, form->username_value),
      ConvertUTF16ToJavaString(env, form->password_value));
}

ScopedJavaLocalRef<jstring> PasswordUIViewAndroid::GetSavedPasswordException(
    JNIEnv* env,
    const JavaRef<jobject>&,
    int index) {
  DCHECK_EQ(State::ALIVE, state_);
  const autofill::PasswordForm* form =
      password_manager_presenter_.GetPasswordException(index);
  if (!form)
    return ConvertUTF8ToJavaString(env, std::string());
  return ConvertUTF8ToJavaString(
      env, password_manager::GetShownOriginAndLinkUrl(*form).first);
}

void PasswordUIViewAndroid::HandleRemoveSavedPasswordEntry(
    JNIEnv* env,
    const JavaRef<jobject>&,
    int index) {
  DCHECK_EQ(State::ALIVE, state_);
  password_manager_presenter_.RemoveSavedPassword(index);
}

void PasswordUIViewAndroid::HandleRemoveSavedPasswordException(
    JNIEnv* env,
    const JavaRef<jobject>&,
    int index) {
  DCHECK_EQ(State::ALIVE, state_);
  password_manager_presenter_.RemovePasswordException(index);
}

void PasswordUIViewAndroid::HandleSerializePasswords(
    JNIEnv* env,
    const JavaRef<jobject>&,
    const JavaRef<jstring>& java_target_directory,
    const JavaRef<jobject>& success_callback,
    const JavaRef<jobject>& error_callback) {
  switch (state_) {
    case State::ALIVE:
      state_ = State::ALIVE_SERIALIZATION_PENDING;
      break;
    case State::ALIVE_SERIALIZATION_PENDING:
      // The UI should not allow the user to re-request export before finishing
      // or cancelling the pending one.
      NOTREACHED();
      return;
    case State::DELETION_PENDING:
      // The Java part should not first request destroying of |this| and then
      // ask |this| for serialized passwords.
      NOTREACHED();
      return;
  }
  // The tasks are posted with base::Unretained, because deletion is postponed
  // until the reply arrives (look for the occurrences of DELETION_PENDING in
  // this file). The background processing is not expected to take very long,
  // but still long enough not to block the UI thread. The main concern here is
  // not to avoid the background computation if |this| is about to be deleted
  // but to simply avoid use after free from the background task runner.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(
          &PasswordUIViewAndroid::ObtainAndSerializePasswords,
          base::Unretained(this),
          base::FilePath(ConvertJavaStringToUTF8(env, java_target_directory))),
      base::BindOnce(&PasswordUIViewAndroid::PostSerializedPasswords,
                     base::Unretained(this),
                     base::android::ScopedJavaGlobalRef<jobject>(
                         env, success_callback.obj()),
                     base::android::ScopedJavaGlobalRef<jobject>(
                         env, error_callback.obj())));
}

void PasswordUIViewAndroid::HandleShowPasswordEntryEditingView(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& context,
    int index) {
  PasswordEditingBridge::LaunchPasswordEntryEditor(
      env, context, GetProfile(),
      password_manager_presenter_.GetPasswords(index),
      password_manager_presenter_.GetUsernamesForRealm(index));
}

ScopedJavaLocalRef<jstring> JNI_PasswordUIView_GetAccountDashboardURL(
    JNIEnv* env) {
  return ConvertUTF16ToJavaString(
      env, l10n_util::GetStringUTF16(IDS_PASSWORDS_WEB_LINK));
}

jboolean JNI_PasswordUIView_HasAccountForLeakCheckRequest(JNIEnv* env) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          ProfileManager::GetLastUsedProfile());
  return password_manager::AuthenticatedLeakCheck::HasAccountForRequest(
      identity_manager);
}

// static
static jlong JNI_PasswordUIView_Init(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  PasswordUIViewAndroid* controller = new PasswordUIViewAndroid(env, obj.obj());
  return reinterpret_cast<intptr_t>(controller);
}

PasswordUIViewAndroid::SerializationResult
PasswordUIViewAndroid::ObtainAndSerializePasswords(
    const base::FilePath& target_directory) {
  // This is run on a backend task runner. Do not access any member variables
  // except for |credential_provider_for_testing_| and
  // |password_manager_presenter_|.
  password_manager::CredentialProviderInterface* const provider =
      credential_provider_for_testing_ ? credential_provider_for_testing_
                                       : &password_manager_presenter_;

  std::vector<std::unique_ptr<autofill::PasswordForm>> passwords =
      provider->GetAllPasswords();

  // The UI should not trigger serialization if there are not passwords.
  DCHECK(!passwords.empty());

  // Creating a file will block the execution on I/O.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // Ensure that the target directory exists.
  base::File::Error error = base::File::FILE_OK;
  if (!base::CreateDirectoryAndGetError(target_directory, &error)) {
    return {0, std::string(), base::File::ErrorToString(error)};
  }

  // Create a temporary file in the target directory to hold the serialized
  // passwords.
  base::FilePath export_file;
  if (!base::CreateTemporaryFileInDir(target_directory, &export_file)) {
    return {
        0, std::string(),
        logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode())};
  }

  // Write the serialized data in CSV.
  std::string data =
      password_manager::PasswordCSVWriter::SerializePasswords(passwords);
  int bytes_written = base::WriteFile(export_file, data.data(), data.size());
  if (bytes_written != base::checked_cast<int>(data.size())) {
    return {
        0, std::string(),
        logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode())};
  }

  return {static_cast<int>(passwords.size()), export_file.value(),
          std::string()};
}

void PasswordUIViewAndroid::PostSerializedPasswords(
    const base::android::JavaRef<jobject>& success_callback,
    const base::android::JavaRef<jobject>& error_callback,
    SerializationResult serialization_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (state_) {
    case State::ALIVE:
      NOTREACHED();
      break;
    case State::ALIVE_SERIALIZATION_PENDING: {
      state_ = State::ALIVE;
      if (export_target_for_testing_) {
        *export_target_for_testing_ = serialization_result;
      } else {
        if (serialization_result.entries_count) {
          base::android::RunIntStringCallbackAndroid(
              success_callback, serialization_result.entries_count,
              serialization_result.exported_file_path);
        } else {
          base::android::RunStringCallbackAndroid(error_callback,
                                                  serialization_result.error);
        }
      }
      break;
    }
    case State::DELETION_PENDING:
      delete this;
      break;
  }
}
