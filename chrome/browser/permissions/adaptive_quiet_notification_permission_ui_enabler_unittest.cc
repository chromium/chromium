// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using QuietUiEnablingMethod =
    QuietNotificationPermissionUiState::EnablingMethod;

}  // namespace

class AdaptiveQuietNotificationPermissionUiEnablerTest : public testing::Test {
 public:
  AdaptiveQuietNotificationPermissionUiEnablerTest()
      : testing_profile_(std::make_unique<TestingProfile>()) {}
  ~AdaptiveQuietNotificationPermissionUiEnablerTest() override = default;

  AdaptiveQuietNotificationPermissionUiEnablerTest(
      const AdaptiveQuietNotificationPermissionUiEnablerTest&) = delete;
  AdaptiveQuietNotificationPermissionUiEnablerTest& operator=(
      const AdaptiveQuietNotificationPermissionUiEnablerTest&) = delete;

  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
};

TEST_F(AdaptiveQuietNotificationPermissionUiEnablerTest,
       MigrateAdaptiveUsersToCPSS) {
  struct {
    bool quiet_ui_pref_value;
    std::optional<QuietUiEnablingMethod> quiet_ui_enabling_method;
    bool expected_notification_cpss_pref_value;
    bool expected_quiet_ui_pref_value;
  } kTests[] = {
      {true, QuietUiEnablingMethod::kUnspecified, true, false},
      {false, QuietUiEnablingMethod::kUnspecified, true, false},
      {true, QuietUiEnablingMethod::kManual, false, true},
      {false, QuietUiEnablingMethod::kManual, true, false},
      {true, QuietUiEnablingMethod::kAdaptive, true, false},
      {false, QuietUiEnablingMethod::kAdaptive, true, false},
      {true, std::nullopt, true, false},
      {false, std::nullopt, true, false},
  };

  auto* permission_ui_enabler =
      AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(profile());

  for (const auto& test : kTests) {
    auto* pref_service = profile()->GetPrefs();
    pref_service->ClearPref(prefs::kEnableNotificationCPSS);
    pref_service->SetBoolean(
        prefs::kDidMigrateAdaptiveNotifiationQuietingToCPSS, false);
    pref_service->SetBoolean(prefs::kEnableQuietNotificationPermissionUi,
                             test.quiet_ui_pref_value);
    if (test.quiet_ui_enabling_method.has_value()) {
      pref_service->SetInteger(
          prefs::kQuietNotificationPermissionUiEnablingMethod,
          static_cast<int>(test.quiet_ui_enabling_method.value()));
    } else {
      pref_service->ClearPref(
          prefs::kQuietNotificationPermissionUiEnablingMethod);
    }
    permission_ui_enabler
        ->MigrateAdaptiveNotificationQuietingToCPSSForTesting();

    EXPECT_EQ(
        test.expected_quiet_ui_pref_value,
        pref_service->GetBoolean(prefs::kEnableQuietNotificationPermissionUi));
    EXPECT_EQ(test.expected_notification_cpss_pref_value,
              pref_service->GetBoolean(prefs::kEnableNotificationCPSS));
  }
}
