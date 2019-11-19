// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"

#include <stdint.h>

#include <memory>

#include "base/bit_cast.h"
#include "base/metrics/field_trial.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

class TriggeredProfileResetterTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.OverrideIsNewProfile(false);
    profile_ = profile_builder.Build();

    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    // Activate the triggered reset field trial for these tests.
    base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
        "TriggeredResetFieldTrial", "On");
    trial->group();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  void SetRegTimestampAndToolName(const base::string16& toolname,
                                  FILETIME* file_time) {
    RegKey trigger_key(HKEY_CURRENT_USER, kTriggeredResetRegistryPath,
                       KEY_ALL_ACCESS);
    ASSERT_TRUE(trigger_key.Valid());
    FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);
    ASSERT_TRUE(trigger_key.WriteValue(kTriggeredResetTimestamp, &ft,
                                       sizeof(ft), REG_QWORD) == ERROR_SUCCESS);
    ASSERT_TRUE(trigger_key.WriteValue(kTriggeredResetToolName,
                                       toolname.c_str()) == ERROR_SUCCESS);
    if (file_time)
      *file_time = ft;
  }

 private:
  registry_util::RegistryOverrideManager override_manager_;
};

TEST_F(TriggeredProfileResetterTest, HasResetTriggerAndClear) {
  SetRegTimestampAndToolName(base::string16(), nullptr);
  TriggeredProfileResetter triggered_profile_resetter(profile_.get());
  triggered_profile_resetter.Activate();
  EXPECT_TRUE(triggered_profile_resetter.HasResetTrigger());
  triggered_profile_resetter.ClearResetTrigger();
  EXPECT_FALSE(triggered_profile_resetter.HasResetTrigger());
}

TEST_F(TriggeredProfileResetterTest, HasDuplicateResetTrigger) {
  FILETIME ft = {};
  SetRegTimestampAndToolName(base::string16(), &ft);
  profile_->GetPrefs()->SetInt64(prefs::kLastProfileResetTimestamp,
                                 bit_cast<int64_t, FILETIME>(ft));

  TriggeredProfileResetter triggered_profile_resetter(profile_.get());
  triggered_profile_resetter.Activate();
  EXPECT_FALSE(triggered_profile_resetter.HasResetTrigger());
}

TEST_F(TriggeredProfileResetterTest, HasToolName) {
  const wchar_t kToolName[] = L"ToolyMcTool";
  SetRegTimestampAndToolName(kToolName, nullptr);
  TriggeredProfileResetter triggered_profile_resetter(profile_.get());
  triggered_profile_resetter.Activate();
  EXPECT_TRUE(triggered_profile_resetter.HasResetTrigger());
  EXPECT_STREQ(kToolName,
               triggered_profile_resetter.GetResetToolName().c_str());
}

TEST_F(TriggeredProfileResetterTest, HasLongToolName) {
  const wchar_t kLongToolName[] =
      L"ToolMcToolToolMcToolToolMcToolToolMcToolToolMcToolToolMcToolToolMcTool"
      L"ToolMcToolToolMcToolToolMcToolThisIsTheToolThatNeverEndsYesItGoesOnAnd"
      L"OnMyFriend";
  const wchar_t kExpectedToolName[] =
      L"ToolMcToolToolMcToolToolMcToolToolMcToolToolMcToolToolMcToolToolMcTool"
      L"ToolMcToolToolMcToolToolMcTool";
  SetRegTimestampAndToolName(kLongToolName, nullptr);
  TriggeredProfileResetter triggered_profile_resetter(profile_.get());
  triggered_profile_resetter.Activate();
  EXPECT_TRUE(triggered_profile_resetter.HasResetTrigger());
  EXPECT_STREQ(kExpectedToolName,
               triggered_profile_resetter.GetResetToolName().c_str());
}
