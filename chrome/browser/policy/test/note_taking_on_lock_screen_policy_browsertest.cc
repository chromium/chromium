// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "chrome/browser/policy/extension_policy_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"

namespace policy {

class NoteTakingOnLockScreenPolicyTest : public ExtensionPolicyTestBase {
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
    ExtensionPolicyTestBase::SetUpCommandLine(command_line);
  }

  void SetUserLevelPrefValue(const std::string& app_id,
                             bool enabled_on_lock_screen) {
    auto* helper = ash::NoteTakingHelper::Get();
    ASSERT_TRUE(helper);

    helper->SetPreferredApp(browser()->profile(), app_id);
    helper->SetPreferredAppEnabledOnLockScreen(browser()->profile(),
                                               enabled_on_lock_screen);
  }

  void SetPolicyValue(std::optional<base::Value> value) {
    PolicyMap policies;
    if (value) {
      policies.Set(key::kNoteTakingAppsLockScreenAllowlist,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, std::move(value), nullptr);
    }
    UpdateProviderPolicy(policies);
  }

  ash::LockScreenAppSupport GetLockScreenSupportForApp(
      const std::string& app_id) {
    return ash::LockScreenApps::GetSupport(browser()->profile(), app_id);
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
  EXPECT_EQ(ash::LockScreenAppSupport::kEnabled,
            GetLockScreenSupportForApp(app->id()));

  SetPolicyValue(base::Value(base::Value::Type::LIST));
  EXPECT_EQ(ash::LockScreenAppSupport::kNotAllowedByPolicy,
            GetLockScreenSupportForApp(app->id()));

  SetPolicyValue(std::nullopt);
  EXPECT_EQ(ash::LockScreenAppSupport::kEnabled,
            GetLockScreenSupportForApp(app->id()));
}

IN_PROC_BROWSER_TEST_F(NoteTakingOnLockScreenPolicyTest,
                       AllowlistLockScreenNoteTakingAppByPolicy) {
  scoped_refptr<const extensions::Extension> app =
      LoadUnpackedExtension("lock_screen_apps/app_launch");
  ASSERT_TRUE(app);
  ASSERT_EQ(kTestAppId, app->id());

  SetUserLevelPrefValue(app->id(), false);
  EXPECT_EQ(ash::LockScreenAppSupport::kSupported,
            GetLockScreenSupportForApp(app->id()));

  base::Value::List policy;
  policy.Append(kTestAppId);
  SetPolicyValue(base::Value(std::move(policy)));

  EXPECT_EQ(ash::LockScreenAppSupport::kSupported,
            GetLockScreenSupportForApp(app->id()));

  SetUserLevelPrefValue(app->id(), true);
  EXPECT_EQ(ash::LockScreenAppSupport::kEnabled,
            GetLockScreenSupportForApp(app->id()));
}

}  // namespace policy
