// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_mode/fake_cws_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {

namespace {

using input_method::InputMethodManager;

// See supported method IDs in //chromeos/ime/input_methods.txt.
constexpr std::string_view kUsEngMethodId = "xkb:us::eng";
constexpr std::string_view kBrPorMethodId = "xkb:br::por";
constexpr std::string_view kFrFraMethodId = "xkb:fr::fra";

// Corresponds to the Chrome extension under:
// //chrome/test/data/chromeos/app_mode/apps_and_extensions/input_extension/src
//
// This extension exercises the `chrome.enterprise.kioskInput` API.
constexpr std::string_view kExtensionId = "elhlgakmbkdkoajdlnfkhjbpgmbpjdig";

// Serves the input extension in `private_cws` and configures `user_policy` to
// force install it.
void ConfigureInputExtension(FakeCwsMixin& private_cws,
                             ScopedUserPolicyUpdate& user_policy) {
  constexpr std::string_view kVersion = "1.0.0";
  auto crx_file = base::StrCat({kExtensionId, "-", kVersion, ".crx"});
  auto policy_entry =
      base::StrCat({kExtensionId, ";", private_cws.UpdateUrl().spec()});

  private_cws.fake_cws().SetUpdateCrx(kExtensionId, crx_file, kVersion);
  user_policy.policy_payload()
      ->mutable_extensioninstallforcelist()
      ->mutable_value()
      ->add_entries(policy_entry);
}

std::string CurrentInputMethod() {
  auto& manager = CHECK_DEREF(InputMethodManager::Get());
  InputMethodManager::State& state =
      CHECK_DEREF(manager.GetActiveIMEState().get());
  return state.GetCurrentInputMethod().id();
}

std::string ToExtensionBasedInputMethod(std::string_view method) {
  auto& manager = CHECK_DEREF(InputMethodManager::Get());
  std::vector<std::string> extension_based_input_methods{std::string(method)};
  CHECK(manager.GetMigratedInputMethodIDs(&extension_based_input_methods));
  return extension_based_input_methods[0];
}

MATCHER_P(HasMethodId, input_method, "") {
  return arg == ToExtensionBasedInputMethod(input_method);
}

bool EnableInputMethods(const std::vector<std::string_view>& methods) {
  auto& manager = CHECK_DEREF(InputMethodManager::Get());
  InputMethodManager::State& state =
      CHECK_DEREF(manager.GetActiveIMEState().get());
  std::vector<std::string> extension_based_input_methods(methods.begin(),
                                                         methods.end());
  return manager.GetMigratedInputMethodIDs(&extension_based_input_methods) &&
         state.ReplaceEnabledInputMethods(extension_based_input_methods);
}

content::EvalJsResult SendMessageToExtension(content::WebContents& web_contents,
                                             std::string message,
                                             std::string data) {
  constexpr std::string_view kScript = R"(
    chrome.runtime.sendMessage($1, { message: $2, data: $3 })
  )";
  return content::EvalJs(
      &web_contents, content::JsReplace(kScript, kExtensionId, message, data));
}

content::EvalJsResult IsExtensionApiAvailable(
    content::WebContents& web_contents) {
  return SendMessageToExtension(web_contents,
                                /*message=*/"is_api_available",
                                /*data=*/std::string());
}

content::EvalJsResult SetInputMethodViaExtension(
    content::WebContents& web_contents,
    std::string_view input_method) {
  return SendMessageToExtension(web_contents,
                                /*message=*/"set_input_method",
                                /*data=*/std::string(input_method));
}

std::string CouldNotChangeInputError(std::string_view input_method) {
  return "Could not change current input method. Invalid input method id: " +
         std::string(input_method) + ".";
}

// The app used to configure `KioskMixin`.
KioskMixin::DefaultServerWebAppOption AppInConfig() {
  return KioskMixin::SimpleWebAppOption();
}

}  // namespace

class KioskEnterpriseInputApiBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  KioskEnterpriseInputApiBrowserTest() = default;
  KioskEnterpriseInputApiBrowserTest(
      const KioskEnterpriseInputApiBrowserTest&) = delete;
  KioskEnterpriseInputApiBrowserTest& operator=(
      const KioskEnterpriseInputApiBrowserTest&) = delete;

  ~KioskEnterpriseInputApiBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    ConfigureInputExtension(
        private_cws_,
        *kiosk_.device_state_mixin().RequestDeviceLocalAccountPolicyUpdate(
            AppInConfig().account_id));
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ExtensionTestMessageListener extension_ready("ready");
    extension_ready.set_extension_id(std::string(kExtensionId));
    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    ASSERT_TRUE(kiosk::test::WaitKioskLaunched());
    SetBrowser(browser_created_observer.Wait());
    // `chrome.runtime` only gets defined once the page finishes loading.
    ASSERT_TRUE(WaitForLoadStop(&GetAppWebContents()));
    ASSERT_TRUE(extension_ready.WaitUntilSatisfied());
  }

  content::WebContents& GetAppWebContents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return CHECK_DEREF(browser_view->GetActiveWebContents());
  }

 private:
  FakeCwsMixin private_cws_{&mixin_host_, FakeCwsMixin::kPrivate};

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/
                    {/*name=*/{},
                     KioskMixin::AutoLaunchAccount{AppInConfig().account_id},
                     {AppInConfig()}}};
};

IN_PROC_BROWSER_TEST_F(KioskEnterpriseInputApiBrowserTest,
                       CompanionExtensionCanAccessApi) {
  ASSERT_EQ(true, IsExtensionApiAvailable(GetAppWebContents()));
}

IN_PROC_BROWSER_TEST_F(KioskEnterpriseInputApiBrowserTest,
                       CurrentMethodDefaultsToFirstEnabledMethod) {
  EXPECT_TRUE(EnableInputMethods({kFrFraMethodId, kBrPorMethodId}));
  EXPECT_THAT(CurrentInputMethod(), HasMethodId(kFrFraMethodId));

  EXPECT_TRUE(EnableInputMethods({kBrPorMethodId, kUsEngMethodId}));
  EXPECT_THAT(CurrentInputMethod(), HasMethodId(kBrPorMethodId));
}

IN_PROC_BROWSER_TEST_F(KioskEnterpriseInputApiBrowserTest,
                       CanChangeToEnabledInputMethod) {
  std::string_view enabled_method_id = kBrPorMethodId;
  EXPECT_TRUE(EnableInputMethods({kUsEngMethodId, enabled_method_id}));

  EXPECT_THAT(CurrentInputMethod(), HasMethodId(kUsEngMethodId));

  EXPECT_EQ(true,
            SetInputMethodViaExtension(GetAppWebContents(), enabled_method_id));
  EXPECT_THAT(CurrentInputMethod(), HasMethodId(enabled_method_id));
}

IN_PROC_BROWSER_TEST_F(KioskEnterpriseInputApiBrowserTest,
                       CannotChangeToDisabledInputMethod) {
  EXPECT_TRUE(EnableInputMethods({kUsEngMethodId, kFrFraMethodId}));

  EXPECT_THAT(CurrentInputMethod(), HasMethodId(kUsEngMethodId));

  std::string_view disabled_method_id = kBrPorMethodId;
  EXPECT_EQ(
      CouldNotChangeInputError(disabled_method_id),
      SetInputMethodViaExtension(GetAppWebContents(), disabled_method_id));
  EXPECT_THAT(CurrentInputMethod(), HasMethodId(kUsEngMethodId));
}

}  // namespace ash
