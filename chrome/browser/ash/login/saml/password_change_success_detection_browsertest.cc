// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpMethod;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

constexpr char kPasswordChangePageTemplate[] =
    "<html><body onload='document.forms[0].submit();'>"
    "<form action='{0}' method='post'>"
    "Old password: <input name='op' type='password' /><br>"
    "New password: <input name='np' type='password' /><br>"
    "Confirm new password: <input name='cnp' type='password' /><br>"
    "<input type='submit' value='Submit'>"
    "</form></body></html>";

// Simulates an IdP at where the user can change the password. Redirects the
// user to URLs in the same way as the real IdP would, which we use to detect
// that the password was changed successfully.
// Unlike the real change password page, this one automatically hits submit
// on the change password form as soon as the page loads.
class FakeChangePasswordIdp {
 public:
  FakeChangePasswordIdp() = default;
  FakeChangePasswordIdp& operator=(const FakeChangePasswordIdp&) = delete;
  FakeChangePasswordIdp(const FakeChangePasswordIdp&) = delete;
  ~FakeChangePasswordIdp() = default;

  void SetFormSubmitAction(const std::string& url) {
    form_submit_action_url_ = url;
  }

  void RedirectNextPostTo(const std::string& url) {
    redirect_next_post_url_ = url;
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

 private:
  std::string GetPasswordChangePageHtmlContent();

  std::string form_submit_action_url_;
  std::string redirect_next_post_url_;
};

std::unique_ptr<HttpResponse> FakeChangePasswordIdp::HandleRequest(
    const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();

  if (request.method == HttpMethod::METHOD_POST) {
    if (redirect_next_post_url_ != "") {
      http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      http_response->AddCustomHeader("Location", redirect_next_post_url_);
      redirect_next_post_url_.clear();
      return http_response;
    }
    http_response->set_code(net::HTTP_OK);
    return http_response;
  }

  http_response->set_code(net::HTTP_OK);
  http_response->set_content(GetPasswordChangePageHtmlContent());
  return http_response;
}

std::string FakeChangePasswordIdp::GetPasswordChangePageHtmlContent() {
  std::string result = kPasswordChangePageTemplate;
  std::string place_holder = "{0}";
  result.replace(result.find(place_holder), place_holder.size(),
                 form_submit_action_url_);
  return result;
}

// Waits for an SAML_IDP_PASSWORD_CHANGED event from the
// InSessionPasswordChangeManager.
class PasswordChangeWaiter : public InSessionPasswordChangeManager::Observer {
 public:
  PasswordChangeWaiter() {
    InSessionPasswordChangeManager::Get()->AddObserver(this);
  }

  PasswordChangeWaiter& operator=(const PasswordChangeWaiter&) = delete;
  PasswordChangeWaiter(const PasswordChangeWaiter&) = delete;

  ~PasswordChangeWaiter() override {
    InSessionPasswordChangeManager::Get()->RemoveObserver(this);
  }

  void WaitForPasswordChange() {
    run_loop_.Run();
    ASSERT_TRUE(saml_password_changed_);
  }

  void OnEvent(InSessionPasswordChangeManager::Event event) override {
    if (event ==
        InSessionPasswordChangeManager::Event::SAML_IDP_PASSWORD_CHANGED) {
      saml_password_changed_ = true;
      run_loop_.Quit();
    }
  }

 private:
  bool saml_password_changed_ = false;
  base::RunLoop run_loop_;
};

// Simulates the redirects that Adfs, Azure, and Ping do in the case of
// password change success, and ensures that we detect each one.
class PasswordChangeSuccessDetectionTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  PasswordChangeSuccessDetectionTest() = default;
  PasswordChangeSuccessDetectionTest& operator=(
      const PasswordChangeSuccessDetectionTest&) = delete;
  PasswordChangeSuccessDetectionTest(
      const PasswordChangeSuccessDetectionTest&) = delete;

  void SetUp() override {
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &FakeChangePasswordIdp::HandleRequest, base::Unretained(&fake_idp_)));

    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kSamlPasswordChangeUrl,
                                    embedded_test_server_.base_url().spec());
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    profile->GetPrefs()->SetBoolean(prefs::kSamlInSessionPasswordChangeEnabled,
                                    true);

    password_change_manager_ =
        std::make_unique<InSessionPasswordChangeManager>(profile);
    InSessionPasswordChangeManager::SetForTesting(
        password_change_manager_.get());
  }

  void WaitForPasswordChangeDetected() {
    PasswordChangeWaiter password_change_waiter;
    password_change_waiter.WaitForPasswordChange();
  }

  void TearDownOnMainThread() override {
    InSessionPasswordChangeManager::ResetForTesting();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer embedded_test_server_{
      net::EmbeddedTestServer::Type::TYPE_HTTPS};
  EmbeddedTestServerSetupMixin embedded_test_server_mixin_{
      &mixin_host_, &embedded_test_server_};
  FakeChangePasswordIdp fake_idp_;

  std::unique_ptr<InSessionPasswordChangeManager> password_change_manager_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeSuccessDetectionTest, DetectAdfsSuccess) {
  fake_idp_.SetFormSubmitAction("/adfs/portal/updatepassword/");
  fake_idp_.RedirectNextPostTo("/adfs/portal/updatepassword/?status=0");

  InSessionPasswordChangeManager::Get()->StartInSessionPasswordChange();
  WaitForPasswordChangeDetected();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeSuccessDetectionTest, DetectAzureSuccess) {
  fake_idp_.SetFormSubmitAction("/ChangePassword.aspx");
  fake_idp_.RedirectNextPostTo("/ChangePassword.aspx?ReturnCode=0");

  InSessionPasswordChangeManager::Get()->StartInSessionPasswordChange();
  WaitForPasswordChangeDetected();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeSuccessDetectionTest, DetectPingSuccess) {
  fake_idp_.SetFormSubmitAction(
      "/idp/directory/a/12345/password/chg/67890?returnurl=/Selection");
  fake_idp_.RedirectNextPostTo("/Selection");

  InSessionPasswordChangeManager::Get()->StartInSessionPasswordChange();
  WaitForPasswordChangeDetected();
}

}  // namespace ash
