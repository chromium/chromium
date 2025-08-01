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
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device_management_backend.pb.h"
#include "extensions/common/extension_id.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
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

constexpr std::string_view kCompanionExtensionId =
    "pogfhljmaechjalhkendaaoldheklnmk";

std::unique_ptr<net::test_server::HttpResponse> ServeSimpleHtmlPage(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(
      "<!DOCTYPE html>"
      "<html lang=\"en\">"
      "<head><title>Test Page</title></head>"
      "<body>A simple kiosk web page.</body>"
      "</html>");
  return response;
}

void SetUpDeviceLocalAccountPolicy(
    const std::string& account_id,
    policy::UserPolicyBuilder& user_policy_builder) {
  enterprise_management::PolicyData& policy_data =
      user_policy_builder.policy_data();
  policy_data.set_public_key_version(1);
  policy_data.set_policy_type(
      policy::dm_protocol::kChromePublicAccountPolicyType);
  policy_data.set_username(account_id);
  policy_data.set_settings_entity_id(account_id);
  user_policy_builder.SetDefaultSigningKey();

  auto& session_manager = CHECK_DEREF(FakeSessionManagerClient::Get());
  session_manager.set_device_local_account_policy(
      std::string(account_id), user_policy_builder.GetBlob());
}

std::string CurrentInputMethod() {
  InputMethodManager& manager = CHECK_DEREF(InputMethodManager::Get());
  InputMethodManager::State& state =
      CHECK_DEREF(manager.GetActiveIMEState().get());
  return state.GetCurrentInputMethod().id();
}

std::string ToExtensionBasedInputMethod(std::string_view method) {
  InputMethodManager& manager = CHECK_DEREF(InputMethodManager::Get());
  std::vector<std::string> extension_based_input_methods{std::string(method)};
  CHECK(manager.GetMigratedInputMethodIDs(&extension_based_input_methods));
  return extension_based_input_methods[0];
}

MATCHER_P(HasMethodId, input_method, "") {
  return arg == ToExtensionBasedInputMethod(input_method);
}

bool EnableInputMethods(const std::vector<std::string_view>& methods) {
  InputMethodManager& manager = CHECK_DEREF(InputMethodManager::Get());
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
      &web_contents,
      content::JsReplace(kScript, kCompanionExtensionId, message, data));
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

}  // namespace

class KioskEnterpriseInputApiBrowserTest : public WebKioskBaseTest {
 public:
  KioskEnterpriseInputApiBrowserTest() = default;
  KioskEnterpriseInputApiBrowserTest(
      const KioskEnterpriseInputApiBrowserTest&) = delete;
  KioskEnterpriseInputApiBrowserTest& operator=(
      const KioskEnterpriseInputApiBrowserTest&) = delete;

  ~KioskEnterpriseInputApiBrowserTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    WebKioskBaseTest::SetUpInProcessBrowserTestFixture();
    SetUpDeviceLocalAccountPolicy(app_account_id(),
                                  device_local_acount_policy_builder_);
  }

  void SetUpOnMainThread() override {
    InitializeWebAppServer();
    WebKioskBaseTest::SetUpOnMainThread();
    InitializeRegularOnlineKiosk();
    ForceInstallAndWaitExtensionReady();
  }

  content::WebContents& app_web_contents() {
    SelectFirstBrowser();
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return CHECK_DEREF(browser_view->GetActiveWebContents());
  }

 private:
  void InitializeWebAppServer() {
    web_app_server_.RegisterRequestHandler(
        base::BindRepeating(&ServeSimpleHtmlPage));
    ASSERT_TRUE(web_app_server_handle_ =
                    web_app_server_.StartAndReturnHandle());
    SetAppInstallUrl(web_app_server_.base_url());
  }

  void ForceInstallAndWaitExtensionReady() {
    constexpr std::string_view kCompanionExtensionPath =
        "extensions/api_test/enterprise_kiosk_input/";
    constexpr std::string_view kCompanionExtensionPemPath =
        "extensions/api_test/enterprise_kiosk_input.pem";
    base::FilePath test_data_dir =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);

    extension_force_install_mixin_.InitWithEmbeddedPolicyMixin(
        &app_profile(), &policy_test_server_mixin_,
        &device_local_acount_policy_builder_,
        /*account_id=*/app_account_id(),
        /*policy_type=*/policy::dm_protocol::kChromePublicAccountPolicyType);

    extensions::ExtensionId extension_id;
    bool did_install = extension_force_install_mixin_.ForceInstallFromSourceDir(
        test_data_dir.AppendASCII(kCompanionExtensionPath),
        test_data_dir.AppendASCII(kCompanionExtensionPemPath),
        ExtensionForceInstallMixin::WaitMode::kReadyMessageReceived,
        &extension_id);
    ASSERT_TRUE(did_install);
    ASSERT_EQ(kCompanionExtensionId, extension_id);
  }

  std::string app_account_id() { return web_app_server_.base_url().spec(); }

  Profile& app_profile() {
    return CHECK_DEREF(ProfileManager::GetPrimaryUserProfile());
  }

  net::test_server::EmbeddedTestServer web_app_server_;
  net::test_server::EmbeddedTestServerHandle web_app_server_handle_;

  policy::UserPolicyBuilder device_local_acount_policy_builder_;
  EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(KioskEnterpriseInputApiBrowserTest,
                       CompanionExtensionCanAccessApi) {
  ASSERT_EQ(true, IsExtensionApiAvailable(app_web_contents()));
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
            SetInputMethodViaExtension(app_web_contents(), enabled_method_id));
  EXPECT_THAT(CurrentInputMethod(), HasMethodId(enabled_method_id));
}

IN_PROC_BROWSER_TEST_F(KioskEnterpriseInputApiBrowserTest,
                       CannotChangeToDisabledInputMethod) {
  EXPECT_TRUE(EnableInputMethods({kUsEngMethodId, kFrFraMethodId}));

  EXPECT_THAT(CurrentInputMethod(), HasMethodId(kUsEngMethodId));

  std::string_view disabled_method_id = kBrPorMethodId;
  EXPECT_EQ(CouldNotChangeInputError(disabled_method_id),
            SetInputMethodViaExtension(app_web_contents(), disabled_method_id));
  EXPECT_THAT(CurrentInputMethod(), HasMethodId(kUsEngMethodId));
}

}  // namespace ash
