// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_survey_handler.h"

#include <memory>

#include "base/system/sys_info.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_list.h"

namespace borealis {

class BorealisSurveyHandlerTest : public ChromeAshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    ChromeAshTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
  }

 protected:
  BorealisSurveyHandlerTest() : window_manager_(profile_.get()) {}

  std::unique_ptr<TestingProfile> profile_;
  BorealisWindowManager window_manager_;
};

TEST_F(BorealisSurveyHandlerTest, GetGameIdReturnsCorrectId) {
  BorealisSurveyHandler handler =
      BorealisSurveyHandler(profile_.get(), &window_manager_);
  CreateFakeApp(profile_.get(), "some_app", "steam://rungameid/646570");
  handler.GetGameId(FakeAppId("some_app"));
  EXPECT_EQ(handler.GetGameId(FakeAppId("some_app")).value(), 646570);
}

TEST_F(BorealisSurveyHandlerTest, GetSurveyDataReturnsCorrectData) {
  BorealisSurveyHandler handler =
      BorealisSurveyHandler(profile_.get(), &window_manager_);
  UpdateDisplay("0+0-500x400, 400+0-500x400");
  CreateFakeApp(profile_.get(), "some_app", "steam://rungameid/646570");
  base::flat_map<std::string, std::string> data = handler.GetSurveyData(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_.get()),
      FakeAppId("some_app"), "Some Game", std::optional<int>(646570));
  base::flat_map<std::string, std::string> expected_data = {
      {"appName", "Some Game"},
      {"board", ""},
      {"specs",
       base::StringPrintf("%ldGB; %s",
                          (long)(base::SysInfo::AmountOfPhysicalMemory() /
                                 (1000 * 1000 * 1000)),
                          base::SysInfo::CPUModelName().c_str())},
      {"monitorsInternal", "0"},
      {"monitorsExternal", "2"},
      {"proton", "None"},
      {"steam", "None"},
      {"gameId", "646570"}};
  EXPECT_EQ(data, expected_data);
}
}  // namespace borealis
