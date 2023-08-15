// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_consent_store.h"

#include "ash/constants/ash_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

class EditorConsentStoreTest : public ::testing::Test {
 public:
  EditorConsentStoreTest() = default;
  ~EditorConsentStoreTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(EditorConsentStoreTest, SettingConsentStatusShouldUpdateUserPrefs) {
  TestingProfile profile_;
  EditorConsentStore store(profile_.GetPrefs());

  store.SetConsentStatus(ConsentStatus::kApproved);

  EXPECT_EQ(store.GetConsentStatus(), ConsentStatus::kApproved);
}

}  // namespace
}  // namespace ash::input_method
