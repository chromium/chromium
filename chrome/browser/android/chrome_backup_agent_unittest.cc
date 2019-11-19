// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/values.h"
#include "chrome/browser/android/chrome_backup_agent.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android {

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaBooleanArray;
using base::android::JavaParamRef;

class ChromeBackupAgentTest : public ::testing::Test {
 protected:
  ChromeBackupAgentTest()
      : expected_pref_names_(GetBackupPrefNames()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
        env_(AttachCurrentThread()) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    testing_profile_ =
        testing_profile_manager_.CreateTestingProfile(chrome::kInitialProfile);
    pref_service_ = testing_profile_->GetTestingPrefService();
    registry_ = pref_service_->registry();
    // Register one dummy pref for testing
    registry_->RegisterBooleanPref("dummy", false);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::vector<std::string> expected_pref_names_;
  TestingProfileManager testing_profile_manager_;
  TestingProfile* testing_profile_;
  sync_preferences::TestingPrefServiceSyncable* pref_service_;
  PrefRegistrySimple* registry_;
  JNIEnv* env_;
};

TEST_F(ChromeBackupAgentTest, GetBoolBackupNames) {
  ScopedJavaLocalRef<jobjectArray> result =
      GetBoolBackupNamesForTesting(env_, JavaParamRef<jobject>(nullptr));
  std::vector<std::string> pref_names;
  AppendJavaStringArrayToStringVector(AttachCurrentThread(), result,
                                      &pref_names);
  EXPECT_EQ(expected_pref_names_, pref_names);
}

TEST_F(ChromeBackupAgentTest, GetBoolBackupValues_AllDefault) {
  ScopedJavaLocalRef<jbooleanArray> result =
      GetBoolBackupValuesForTesting(env_, JavaParamRef<jobject>(nullptr));
  std::vector<bool> values;
  JavaBooleanArrayToBoolVector(env_, result, &values);
  ASSERT_EQ(expected_pref_names_.size(), values.size());
  for (size_t i = 0; i < values.size(); i++) {
    bool expected_value;
    ASSERT_TRUE(pref_service_->GetDefaultPrefValue(expected_pref_names_[i])
                    ->GetAsBoolean(&expected_value));
    EXPECT_EQ(expected_value, values[i]) << "i = " << i << ", "
                                         << expected_pref_names_[i];
  }
}

TEST_F(ChromeBackupAgentTest, GetBoolBackupValues_IrrelevantChange) {
  // Try changing the dummy value, should make no difference
  pref_service_->SetBoolean("dummy", true);

  ScopedJavaLocalRef<jbooleanArray> result =
      GetBoolBackupValuesForTesting(env_, JavaParamRef<jobject>(nullptr));
  std::vector<bool> values;
  JavaBooleanArrayToBoolVector(env_, result, &values);
  ASSERT_EQ(expected_pref_names_.size(), values.size());
  for (size_t i = 0; i < values.size(); i++) {
    bool expected_value;
    ASSERT_TRUE(pref_service_->GetDefaultPrefValue(expected_pref_names_[i])
                    ->GetAsBoolean(&expected_value));
    EXPECT_EQ(expected_value, values[i]) << "i = " << i << ", "
                                         << expected_pref_names_[i];
  }
}

TEST_F(ChromeBackupAgentTest, GetBoolBackupValues_RelevantChange) {
  // Change one of the values we care about
  pref_service_->SetBoolean(expected_pref_names_[3], false);
  ScopedJavaLocalRef<jbooleanArray> result =
      GetBoolBackupValuesForTesting(env_, JavaParamRef<jobject>(nullptr));
  std::vector<bool> values;
  JavaBooleanArrayToBoolVector(env_, result, &values);
  ASSERT_EQ(expected_pref_names_.size(), values.size());
  for (size_t i = 0; i < values.size(); i++) {
    EXPECT_EQ(pref_service_->GetBoolean(expected_pref_names_[i]), values[i])
        << "i = " << i << ", " << expected_pref_names_[i];
  }
}

TEST_F(ChromeBackupAgentTest, SetBoolBackupValues) {
  ScopedJavaLocalRef<jobjectArray> narray =
      ToJavaArrayOfStrings(env_, expected_pref_names_);
  bool* values = new bool[expected_pref_names_.size()];
  for (size_t i = 0; i < expected_pref_names_.size(); i++) {
    values[i] = false;
  }
  // Set a couple of the values to true.
  values[5] = true;
  values[8] = true;
  ScopedJavaLocalRef<jbooleanArray> varray =
      ToJavaBooleanArray(env_, values, expected_pref_names_.size());
  SetBoolBackupPrefsForTesting(env_, JavaParamRef<jobject>(nullptr),
                               JavaParamRef<jobjectArray>(env_, narray.obj()),
                               JavaParamRef<jbooleanArray>(env_, varray.obj()));
  for (size_t i = 0; i < expected_pref_names_.size(); i++) {
    EXPECT_EQ(values[i], pref_service_->GetBoolean(expected_pref_names_[i]))
        << "i = " << i << ", " << expected_pref_names_[i];
  }
  EXPECT_FALSE(pref_service_->GetBoolean("dummy"));
}

}  //  namespace android
