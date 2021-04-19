// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"

namespace policy {

class NoteTakingOnLockScreenPolicyTest : public PolicyTest {
 public:
  NoteTakingOnLockScreenPolicyTest() = default;
  ~NoteTakingOnLockScreenPolicyTest() override = default;
  NoteTakingOnLockScreenPolicyTest(
      const NoteTakingOnLockScreenPolicyTest& other) = delete;
  NoteTakingOnLockScreenPolicyTest& operator=(
      const NoteTakingOnLockScreenPolicyTest& other) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // An app requires lockScreen permission to be enabled as a lock screen app.
    // This permission is protected by a allowlist, so the test app has to be
    // allowlisted as well.
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kTestAppId);
    command_line->AppendSwitch(ash::switches::kAshForceEnableStylusTools);
    PolicyTest::SetUpCommandLine(command_line);
  }

  void SetUserLevelPrefValue(const std::string& app_id,
                             bool enabled_on_lock_screen) {
    chromeos::NoteTakingHelper* helper = chromeos::NoteTakingHelper::Get();
    ASSERT_TRUE(helper);

    helper->SetPreferredApp(browser()->profile(), app_id);
    helper->SetPreferredAppEnabledOnLockScreen(browser()->profile(),
                                               enabled_on_lock_screen);
  }

  void SetPolicyValue(base::Optional<base::Value> value) {
    PolicyMap policies;
    if (value) {
      policies.Set(key::kNoteTakingAppsLockScreenAllowlist,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, std::move(value), nullptr);
    }
    UpdateProviderPolicy(policies);
  }

  chromeos::NoteTakingLockScreenSupport GetAppLockScreenStatus(
      const std::string& app_id) {
    std::unique_ptr<chromeos::NoteTakingAppInfo> info =
        chromeos::NoteTakingHelper::Get()->GetPreferredLockScreenAppInfo(
            browser()->profile());
    if (!info || info->app_id != app_id)
      return chromeos::NoteTakingLockScreenSupport::kNotSupported;
    return info->lock_screen_support;
  }

  // The test app ID.
  static const char kTestAppId[];
};

const char NoteTakingOnLockScreenPolicyTest::kTestAppId[] =
    "cadfeochfldmbdgoccgbeianhamecbae";

IN_PROC_BROWSER_TEST_F(NoteTakingOnLockScreenPolicyTest,
                       DisableLockScreenNoteTakingByPolicy) {
  scoped_refptr<const extensions::Extension> app =
      LoadUnpackedExtension("lock_screen_apps/app_launch");
  ASSERT_TRUE(app);
  ASSERT_EQ(kTestAppId, app->id());

  SetUserLevelPrefValue(app->id(), true);
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kEnabled,
            GetAppLockScreenStatus(app->id()));

  SetPolicyValue(base::Value(base::Value::Type::LIST));
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kNotAllowedByPolicy,
            GetAppLockScreenStatus(app->id()));

  SetPolicyValue(base::nullopt);
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kEnabled,
            GetAppLockScreenStatus(app->id()));
}

IN_PROC_BROWSER_TEST_F(NoteTakingOnLockScreenPolicyTest,
                       AllowlistLockScreenNoteTakingAppByPolicy) {
  scoped_refptr<const extensions::Extension> app =
      LoadUnpackedExtension("lock_screen_apps/app_launch");
  ASSERT_TRUE(app);
  ASSERT_EQ(kTestAppId, app->id());

  SetUserLevelPrefValue(app->id(), false);
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kSupported,
            GetAppLockScreenStatus(app->id()));

  base::Value policy(base::Value::Type::LIST);
  policy.Append(kTestAppId);
  SetPolicyValue(std::move(policy));

  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kSupported,
            GetAppLockScreenStatus(app->id()));

  SetUserLevelPrefValue(app->id(), true);
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kEnabled,
            GetAppLockScreenStatus(app->id()));
}

}  // namespace policy
