// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/test/app_window_waiter.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/extensions/api/enterprise_kiosk_input/enterprise_kiosk_input_api.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {

namespace {

const char kTestEnterpriseServiceAccountEmail[] =
    "service_account@system.gserviceaccount.com";
const char kUsEngKeyboardId[] = "xkb:us::eng";
const char kLatamSpaKeyboardId[] = "xkb:latam::spa";
const char kFraFrKeyboardId[] = "xkb:fra::fr";
constexpr char kSetInputMethodTestTemplate[] = R"(
    const kioskInput = chrome.enterprise.kioskInput;
    new Promise((resolve, reject) => {
      const options = {
        inputMethodId: '%s',
      };
      kioskInput.setCurrentInputMethod(
        options, () => {
          if(chrome.runtime.lastError) {
            reject(chrome.runtime.lastError.message);
          } else {
            resolve('DONE');
          }
        });
    });)";
constexpr char kSetInputMethodTestErrorMessageTemplate[] =
    "a JavaScript error: \"Could not change current input method. Invalid "
    "input method id: %s.\"\n";

}  // namespace

class KioskEnterpriseInputApiBrowserTest : public KioskBaseTest {
 public:
  KioskEnterpriseInputApiBrowserTest(
      const KioskEnterpriseInputApiBrowserTest&) = delete;
  KioskEnterpriseInputApiBrowserTest& operator=(
      const KioskEnterpriseInputApiBrowserTest&) = delete;

 protected:
  KioskEnterpriseInputApiBrowserTest() { set_use_consumer_kiosk_mode(false); }

  void ConfigureKioskAppInPolicy(const std::string& account_id,
                                 const std::string& app_id,
                                 const std::string& update_url) {
    std::vector<policy::DeviceLocalAccount> accounts;
    accounts.emplace_back(policy::DeviceLocalAccount::TYPE_KIOSK_APP,
                          policy::DeviceLocalAccount::EphemeralMode::kUnset,
                          account_id, app_id, update_url);
    policy::SetDeviceLocalAccounts(owner_settings_service_.get(), accounts);
    settings_helper_.SetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                               account_id);
    settings_helper_.SetString(kServiceAccountIdentity,
                               kTestEnterpriseServiceAccountEmail);
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(KioskEnterpriseInputApiBrowserTest,
                       SetCurrentInputMethodTest) {
  // Prepare Fake CWS to serve app crx.
  SetTestApp(kTestEnterpriseKioskAppId);
  SetupTestAppUpdateCheck();

  // Configure `kTestEnterpriseKioskAppId` in device policy.
  ConfigureKioskAppInPolicy(kTestEnterpriseAccountId, kTestEnterpriseKioskAppId,
                            /*update_url=*/"");

  PrepareAppLaunch();
  EXPECT_TRUE(LaunchApp(kTestEnterpriseKioskAppId));

  KioskSessionInitializedWaiter().Wait();

  // Check installer status.
  EXPECT_EQ(KioskAppLaunchError::Error::kNone, KioskAppLaunchError::Get());
  EXPECT_EQ(ManifestLocation::kExternalPolicy, GetInstalledAppLocation());

  // Enable two input methods: en_US and es_LATAM.
  auto* imm = ash::input_method::InputMethodManager::Get();
  ASSERT_TRUE(imm);
  scoped_refptr<ash::input_method::InputMethodManager::State> ime_state =
      imm->GetActiveIMEState();
  ASSERT_TRUE(ime_state.get());
  // This step is needed to convert the input method IDs to the input manager
  // IDs.
  std::vector<std::string> migrated_input_methods(
      {kUsEngKeyboardId, kLatamSpaKeyboardId});
  EXPECT_TRUE(imm->MigrateInputMethods(&migrated_input_methods));
  for (const auto& method : migrated_input_methods) {
    EXPECT_TRUE(ime_state->EnableInputMethod(method));
  }
  const std::string& migrated_us_keyboard = migrated_input_methods[0];
  const std::string& migrated_latam_keyboard = migrated_input_methods[1];

  // Verify that the two input methods are enabled.
  EXPECT_EQ(migrated_input_methods.size(),
            ime_state->GetEnabledInputMethods().size());
  EXPECT_EQ(migrated_us_keyboard, ime_state->GetCurrentInputMethod().id());

  // Wait for the window to appear.
  extensions::AppWindow* window =
      apps::AppWindowWaiter(extensions::AppWindowRegistry::Get(
                                ProfileManager::GetPrimaryUserProfile()),
                            kTestEnterpriseKioskAppId)
          .Wait();
  ASSERT_TRUE(window);
  EXPECT_TRUE(content::WaitForLoadStop(window->web_contents()));

  // Verify that the app window has access to the kiosk input API.
  EXPECT_EQ("object", content::EvalJs(window->web_contents(),
                                      "typeof chrome.enterprise"));
  EXPECT_EQ("object", content::EvalJs(window->web_contents(),
                                      "typeof chrome.enterprise.kioskInput"));
  EXPECT_EQ("function",
            content::EvalJs(
                window->web_contents(),
                "typeof chrome.enterprise.kioskInput.setCurrentInputMethod"));

  // Try to switch the current input method to es_LATAM, this should succeed.
  EXPECT_EQ("DONE",
            content::EvalJs(window->web_contents(),
                            base::StringPrintf(kSetInputMethodTestTemplate,
                                               kLatamSpaKeyboardId)));
  EXPECT_EQ(migrated_latam_keyboard, ime_state->GetCurrentInputMethod().id());

  // Try to switch the current input method to en_US, this should also succeed.
  EXPECT_EQ("DONE",
            content::EvalJs(window->web_contents(),
                            base::StringPrintf(kSetInputMethodTestTemplate,
                                               kUsEngKeyboardId)));
  EXPECT_EQ(migrated_us_keyboard, ime_state->GetCurrentInputMethod().id());

  // Try to switch the current input method to fr_FRA, this should fail as this
  // languages is not enabled.
  const content::EvalJsResult error_result = content::EvalJs(
      window->web_contents(),
      base::StringPrintf(kSetInputMethodTestTemplate, kFraFrKeyboardId));

  EXPECT_EQ(base::StringPrintf(kSetInputMethodTestErrorMessageTemplate,
                               kFraFrKeyboardId),
            error_result.error);
  EXPECT_EQ(migrated_us_keyboard, ime_state->GetCurrentInputMethod().id());

  // Terminate the app.
  window->GetBaseWindow()->Close();
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
