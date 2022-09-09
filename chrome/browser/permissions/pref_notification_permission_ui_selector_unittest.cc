// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/pref_notification_permission_ui_selector.h"

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

using testing::DoAll;

using QuietUiReason = PrefNotificationPermissionUiSelector::QuietUiReason;
using WarningReason = PrefNotificationPermissionUiSelector::WarningReason;
using Decision = PrefNotificationPermissionUiSelector::Decision;

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

}  // namespace

class PrefNotificationPermissionUiSelectorTest : public testing::Test {
 public:
  PrefNotificationPermissionUiSelectorTest()
      : testing_profile_(std::make_unique<TestingProfile>()),
        pref_selector_(testing_profile_.get()) {}

  PrefNotificationPermissionUiSelectorTest(
      const PrefNotificationPermissionUiSelectorTest&) = delete;
  PrefNotificationPermissionUiSelectorTest& operator=(
      const PrefNotificationPermissionUiSelectorTest&) = delete;

  PrefNotificationPermissionUiSelector* pref_selector() {
    return &pref_selector_;
  }

  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> testing_profile_;

  PrefNotificationPermissionUiSelector pref_selector_;
};

TEST_F(PrefNotificationPermissionUiSelectorTest, FeatureAndPrefCombinations) {
  const struct {
    bool feature_enabled;
    bool quiet_ui_enabled_in_prefs;
    absl::optional<QuietUiReason> expected_reason;
  } kTests[] = {
      {true, false, Decision::UseNormalUi()},
      {true, true, QuietUiReason::kEnabledInPrefs},
      {false, true, Decision::UseNormalUi()},
      {false, false, Decision::UseNormalUi()},
  };

  for (const auto& test_case : kTests) {
    SCOPED_TRACE(base::StringPrintf("feature: %d, pref: %d",
                                    test_case.feature_enabled,
                                    test_case.quiet_ui_enabled_in_prefs));

    // Init feature and pref settings.
    base::test::ScopedFeatureList feature_list;
    if (test_case.feature_enabled)
      feature_list.InitAndEnableFeature(features::kQuietNotificationPrompts);
    else
      feature_list.InitAndDisableFeature(features::kQuietNotificationPrompts);
    profile()->GetPrefs()->SetBoolean(
        prefs::kEnableQuietNotificationPermissionUi,
        test_case.quiet_ui_enabled_in_prefs);

    // Setup and prepare for the request.
    base::RunLoop callback_loop;
    base::MockCallback<
        PrefNotificationPermissionUiSelector::DecisionMadeCallback>
        mock_callback;
    Decision actual_decison(absl::nullopt, absl::nullopt);

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
