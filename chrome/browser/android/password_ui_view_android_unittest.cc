// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_ui_view_android.h"

#include <jni.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace android {

using autofill::PasswordForm;
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace {

// Specific deleter for PasswordUIViewAndroid, which calls
// PasswordUIViewAndroid::Destroy on the object. Use this with a
// std::unique_ptr.
struct PasswordUIViewAndroidDestroyDeleter {
  inline void operator()(void* ptr) const {
    (static_cast<PasswordUIViewAndroid*>(ptr)->Destroy(
        nullptr, JavaParamRef<jobject>(nullptr)));
  }
};

class FakeCredentialProvider
    : public password_manager::CredentialProviderInterface {
 public:
  FakeCredentialProvider() = default;
  ~FakeCredentialProvider() override = default;

  // password_manager::CredentialProviderInterface
  std::vector<std::unique_ptr<PasswordForm>> GetAllPasswords() override;

  // Adds a PasswordForm specified by the arguments to the list returned by
  // GetAllPasswords.
  void AddPasswordEntry(const std::string& origin,
                        const std::string& username,
                        const std::string& password);

 private:
  std::vector<std::unique_ptr<PasswordForm>> passwords_;

  DISALLOW_COPY_AND_ASSIGN(FakeCredentialProvider);
};

std::vector<std::unique_ptr<PasswordForm>>
FakeCredentialProvider::GetAllPasswords() {
  std::vector<std::unique_ptr<PasswordForm>> clone;
  for (const auto& password : passwords_) {
    clone.push_back(std::make_unique<PasswordForm>(*password));
  }
  return clone;
}

void FakeCredentialProvider::AddPasswordEntry(const std::string& origin,
                                              const std::string& username,
                                              const std::string& password) {
  auto form = std::make_unique<PasswordForm>();
  form->origin = GURL(origin);
  form->signon_realm = origin;
  form->username_value = base::UTF8ToUTF16(username);
  form->password_value = base::UTF8ToUTF16(password);
  passwords_.push_back(std::move(form));
}

}  // namespace

class PasswordUIViewAndroidTest : public ::testing::Test {
 protected:
  PasswordUIViewAndroidTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
        env_(AttachCurrentThread()) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    testing_profile_ =
        testing_profile_manager_.CreateTestingProfile("test profile");

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  TestingProfile* testing_profile_;
  JNIEnv* env_;
  base::ScopedTempDir temp_dir_;
};

// Test that the asynchronous processing of password serialization controlled by
// PasswordUIViewAndroid arrives at the same result as synchronous way to
// serialize the passwords.
TEST_F(PasswordUIViewAndroidTest, GetSerializedPasswords) {
  FakeCredentialProvider provider;
  provider.AddPasswordEntry("https://example.com", "username", "password");

  // Let the PasswordCSVWriter compute the result instead of hard-coding it,
  // because this test focuses on PasswordUIView and not on detecting changes in
  // PasswordCSVWriter.
  const std::string expected_result =
      password_manager::PasswordCSVWriter::SerializePasswords(
          provider.GetAllPasswords());

  std::unique_ptr<PasswordUIViewAndroid, PasswordUIViewAndroidDestroyDeleter>
      password_ui_view(
          new PasswordUIViewAndroid(env_, JavaParamRef<jobject>(nullptr)));
  PasswordUIViewAndroid::SerializationResult serialized_passwords;
  password_ui_view->set_export_target_for_testing(&serialized_passwords);
  password_ui_view->set_credential_provider_for_testing(&provider);
  password_ui_view->HandleSerializePasswords(
      env_, nullptr,
      base::android::ConvertUTF8ToJavaString(
          env_, temp_dir_.GetPath().AsUTF8Unsafe()),
      nullptr, nullptr);

  content::RunAllTasksUntilIdle();
  // The buffer for actual result is 1 byte longer than the expected data to be
  // able to detect when the actual data are too long.
  char actual_result[expected_result.size() + 1];
  int number_of_bytes_read = base::ReadFile(
      base::FilePath::FromUTF8Unsafe(serialized_passwords.exported_file_path),
      actual_result, expected_result.size() + 1);
  EXPECT_EQ(static_cast<int>(expected_result.size()), number_of_bytes_read);
  EXPECT_EQ(expected_result,
            std::string(actual_result,
                        (number_of_bytes_read < 0) ? 0 : number_of_bytes_read));
  EXPECT_EQ(1, serialized_passwords.entries_count);
  EXPECT_FALSE(serialized_passwords.exported_file_path.empty());
  EXPECT_EQ(std::string(), serialized_passwords.error);
}

// Test that destroying the PasswordUIView when tasks are pending does not lead
// to crashes.
TEST_F(PasswordUIViewAndroidTest, GetSerializedPasswords_Cancelled) {
  FakeCredentialProvider provider;
  provider.AddPasswordEntry("https://example.com", "username", "password");

  std::unique_ptr<PasswordUIViewAndroid, PasswordUIViewAndroidDestroyDeleter>
      password_ui_view(
          new PasswordUIViewAndroid(env_, JavaParamRef<jobject>(nullptr)));
  PasswordUIViewAndroid::SerializationResult serialized_passwords;
  serialized_passwords.entries_count = 123;
  serialized_passwords.exported_file_path = "somepath";
  password_ui_view->set_export_target_for_testing(&serialized_passwords);
  password_ui_view->set_credential_provider_for_testing(&provider);
  base::android::ScopedJavaLocalRef<jstring> java_target_dir =
      base::android::ConvertUTF8ToJavaString(
          env_, temp_dir_.GetPath().AsUTF8Unsafe());
  password_ui_view->HandleSerializePasswords(
      env_, nullptr,
      base::android::JavaParamRef<jstring>(env_, java_target_dir.obj()),
      nullptr, nullptr);
  // Register the PasswordUIView for deletion. It should not destruct itself
  // before the background tasks are run. The results of the background tasks
  // are waited for and then thrown out, so |serialized_passwords| should not be
  // overwritten.
  password_ui_view.reset();
  // Now run the background tasks (and the subsequent deletion).
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(123, serialized_passwords.entries_count);
  EXPECT_EQ("somepath", serialized_passwords.exported_file_path);
  EXPECT_EQ(std::string(), serialized_passwords.error);
}

// Test that an I/O error is reported.
TEST_F(PasswordUIViewAndroidTest, GetSerializedPasswords_WriteFailed) {
  FakeCredentialProvider provider;
  provider.AddPasswordEntry("https://example.com", "username", "password");

  std::unique_ptr<PasswordUIViewAndroid, PasswordUIViewAndroidDestroyDeleter>
      password_ui_view(
          new PasswordUIViewAndroid(env_, JavaParamRef<jobject>(nullptr)));
  PasswordUIViewAndroid::SerializationResult serialized_passwords;
  password_ui_view->set_export_target_for_testing(&serialized_passwords);
  password_ui_view->set_credential_provider_for_testing(&provider);
  base::android::ScopedJavaLocalRef<jstring> java_temp_file =
      base::android::ConvertUTF8ToJavaString(
          env_, "/This directory cannot be created");
  password_ui_view->HandleSerializePasswords(
      env_, nullptr,
      base::android::JavaParamRef<jstring>(env_, java_temp_file.obj()), nullptr,
      nullptr);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, serialized_passwords.entries_count);
  EXPECT_FALSE(serialized_passwords.error.empty());
}

}  //  namespace android
