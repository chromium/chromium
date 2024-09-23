// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_ui_view_android.h"

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
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/fixed_array.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/password_manager/core/browser/export/password_csv_writer.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android {

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

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

}  // namespace

class PasswordUIViewAndroidTest : public ::testing::Test {
 protected:
  PasswordUIViewAndroidTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
        env_(AttachCurrentThread()) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    testing_profile_ =
        testing_profile_manager_.CreateTestingProfile("TestProfile");
    profiles::SetLastUsedProfile(testing_profile_->GetBaseName());

    store_ = CreateAndUseTestPasswordStore(testing_profile_);
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
    ASSERT_TRUE(temp_dir().CreateUniqueTempDir());
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();
    RunUntilIdle();
  }

  PasswordForm AddPasswordEntry(const std::string& origin,
                                const std::string& username,
                                const std::string& password) {
    PasswordForm form;
    form.url = GURL(origin);
    form.signon_realm = origin;
    form.username_value = base::UTF8ToUTF16(username);
    form.password_value = base::UTF8ToUTF16(password);
    store_->AddLogin(form);
    RunUntilIdle();
    return form;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  raw_ptr<JNIEnv> env() { return env_; }
  base::ScopedTempDir& temp_dir() { return temp_dir_; }

 protected:
  raw_ptr<TestingProfile> testing_profile_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  scoped_refptr<TestPasswordStore> store_;
  raw_ptr<JNIEnv> env_;
  base::ScopedTempDir temp_dir_;
};

// Test that the asynchronous processing of password serialization controlled by
// PasswordUIViewAndroid arrives at the same result as synchronous way to
// serialize the passwords.
TEST_F(PasswordUIViewAndroidTest, GetSerializedPasswords) {
  PasswordForm form =
      AddPasswordEntry("https://example.com", "username", "password");

  // Let the PasswordCSVWriter compute the result instead of hard-coding it,
  // because this test focuses on PasswordUIView and not on detecting changes in
  // PasswordCSVWriter.
  const std::string expected_result =
      password_manager::PasswordCSVWriter::SerializePasswords(
          {password_manager::CredentialUIEntry(form)});

  std::unique_ptr<PasswordUIViewAndroid, PasswordUIViewAndroidDestroyDeleter>
      password_ui_view(new PasswordUIViewAndroid(
          env(), JavaParamRef<jobject>(nullptr), testing_profile_));
  // SavedPasswordsPresenter needs time to initialize and fetch passwords.
  RunUntilIdle();

  PasswordUIViewAndroid::SerializationResult serialized_passwords;
  password_ui_view->set_export_target_for_testing(&serialized_passwords);
  password_ui_view->HandleSerializePasswords(
      env(), nullptr, temp_dir().GetPath().AsUTF8Unsafe(), nullptr, nullptr);

  content::RunAllTasksUntilIdle();
  // The buffer for actual result is 1 byte longer than the expected data to be
  // able to detect when the actual data are too long.
  base::FixedArray<char> actual_result(expected_result.size() + 1);
  int number_of_bytes_read = base::ReadFile(
      base::FilePath::FromUTF8Unsafe(serialized_passwords.exported_file_path),
      actual_result.data(), expected_result.size() + 1);
  EXPECT_EQ(static_cast<int>(expected_result.size()), number_of_bytes_read);
  EXPECT_EQ(expected_result,
            std::string(actual_result.data(),
                        (number_of_bytes_read < 0) ? 0 : number_of_bytes_read));
  EXPECT_EQ(1, serialized_passwords.entries_count);
  EXPECT_FALSE(serialized_passwords.exported_file_path.empty());
  EXPECT_EQ(std::string(), serialized_passwords.error);
}

// Test that destroying the PasswordUIView when tasks are pending does not lead
// to crashes.
TEST_F(PasswordUIViewAndroidTest, GetSerializedPasswords_Cancelled) {
  AddPasswordEntry("https://example.com", "username", "password");

  std::unique_ptr<PasswordUIViewAndroid, PasswordUIViewAndroidDestroyDeleter>
      password_ui_view(new PasswordUIViewAndroid(
          env(), JavaParamRef<jobject>(nullptr), testing_profile_));
  // SavedPasswordsPresenter needs time to initialize and fetch passwords.
  RunUntilIdle();

  PasswordUIViewAndroid::SerializationResult serialized_passwords;
  serialized_passwords.entries_count = 123;
  serialized_passwords.exported_file_path = "somepath";
  password_ui_view->set_export_target_for_testing(&serialized_passwords);
  password_ui_view->HandleSerializePasswords(
      env(), nullptr, temp_dir().GetPath().AsUTF8Unsafe(), nullptr, nullptr);
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
  AddPasswordEntry("https://example.com", "username", "password");

  std::unique_ptr<PasswordUIViewAndroid, PasswordUIViewAndroidDestroyDeleter>
      password_ui_view(new PasswordUIViewAndroid(
          env(), JavaParamRef<jobject>(nullptr), testing_profile_));
  // SavedPasswordsPresenter needs time to initialize and fetch passwords.
  RunUntilIdle();

  PasswordUIViewAndroid::SerializationResult serialized_passwords;
  password_ui_view->set_export_target_for_testing(&serialized_passwords);
  password_ui_view->HandleSerializePasswords(
      env(), nullptr, "/This directory cannot be created", nullptr, nullptr);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, serialized_passwords.entries_count);
  EXPECT_FALSE(serialized_passwords.error.empty());
}

}  //  namespace android
