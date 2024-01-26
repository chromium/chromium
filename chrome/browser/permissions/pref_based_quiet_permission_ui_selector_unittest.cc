// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/pref_based_quiet_permission_ui_selector.h"

#include <optional>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::DoAll;

using QuietUiReason = PrefBasedQuietPermissionUiSelector::QuietUiReason;
using WarningReason = PrefBasedQuietPermissionUiSelector::WarningReason;
using Decision = PrefBasedQuietPermissionUiSelector::Decision;

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

}  // namespace

class PrefBasedQuietPermissionUiSelectorTest : public testing::Test {
 public:
  PrefBasedQuietPermissionUiSelectorTest()
      : testing_profile_(std::make_unique<TestingProfile>()),
        pref_selector_(testing_profile_.get()) {}

  PrefBasedQuietPermissionUiSelectorTest(
      const PrefBasedQuietPermissionUiSelectorTest&) = delete;
  PrefBasedQuietPermissionUiSelectorTest& operator=(
      const PrefBasedQuietPermissionUiSelectorTest&) = delete;

  PrefBasedQuietPermissionUiSelector* pref_selector() {
    return &pref_selector_;
  }

  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> testing_profile_;

  PrefBasedQuietPermissionUiSelector pref_selector_;
};

TEST_F(PrefBasedQuietPermissionUiSelectorTest, FeatureAndPrefCombinations) {
  const struct {
    bool quiet_ui_enabled_in_prefs;
    std::optional<QuietUiReason> expected_reason;
  } kTests[] = {
      {false, Decision::UseNormalUi()},
      {true, QuietUiReason::kEnabledInPrefs},
  };

  for (const auto& test_case : kTests) {
    SCOPED_TRACE(
        base::StringPrintf("pref: %d", test_case.quiet_ui_enabled_in_prefs));

    // Init pref settings.
    profile()->GetPrefs()->SetBoolean(
        prefs::kEnableQuietNotificationPermissionUi,
        test_case.quiet_ui_enabled_in_prefs);

    // Setup and prepare for the request.
    base::RunLoop callback_loop;
    base::MockCallback<PrefBasedQuietPermissionUiSelector::DecisionMadeCallback>
        mock_callback;
    Decision actual_decison(std::nullopt, std::nullopt);

    // Make a request and wait for the callback.
    EXPECT_CALL(mock_callback, Run)
        .WillRepeatedly(DoAll(testing::SaveArg<0>(&actual_decison),
                              QuitMessageLoop(&callback_loop)));

    permissions::MockPermissionRequest mock_request(
        GURL("http://example.com"), permissions::RequestType::kNotifications);
    pref_selector()->SelectUiToUse(&mock_request, mock_callback.Get());
    callback_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_callback);

    // Check expectations.
    EXPECT_EQ(test_case.expected_reason, actual_decison.quiet_ui_reason);
    EXPECT_EQ(Decision::ShowNoWarning(), actual_decison.warning_reason);
  }
}
