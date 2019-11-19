// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager_base.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/browser/consistency_cookie_manager_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/canonical_cookie.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kConsistencyCookieName[] = "CHROME_ID_CONSISTENCY_STATE";

// Subclass of ConsistencyCookieManagerBase allowing to manually control the
// value of the cookie.
class TestConsistencyCookieManager
    : public signin::ConsistencyCookieManagerBase,
      public network::mojom::CookieChangeListener {
 public:
  TestConsistencyCookieManager(SigninClient* client,
                               AccountReconcilor* reconcilor)
      : signin::ConsistencyCookieManagerBase(client, reconcilor) {
    // Listen to cookie changes.
    client->GetCookieManager()->AddGlobalChangeListener(
        cookie_listener_receiver_.BindNewPipeAndPassRemote());
    // Subclasses have to call UpdateCookie() in the constructor.
    UpdateCookie();
    // Wait for the initial cookie to be set.
    WaitForCookieChange();
  }

  // Sets a new value for the consistency cookie.
  void SetValue(const std::string& value) {
    value_ = value;
    UpdateCookie();
    WaitForCookieChange();
  }

 private:
  // Waits until OnCookieChange is called.
  void WaitForCookieChange() {
    base::RunLoop loop;
    run_loop_quit_closure_ = loop.QuitClosure();
    loop.Run();
  }

  // CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override {
    if (change.cookie.Name() != kConsistencyCookieName)
      return;
    if (!run_loop_quit_closure_.is_null())
      std::move(run_loop_quit_closure_).Run();
  }

  // ConsistencyCookieManagerBase:
  std::string CalculateCookieValue() override { return value_; }

  std::string value_ = "initial_value";
  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};
  base::OnceClosure run_loop_quit_closure_;
};

}  // namespace

class ConsistencyCookieBrowserTest : public InProcessBrowserTest {
 public:
  ConsistencyCookieBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    net::test_server::RegisterDefaultHandlers(&https_server_);
  }

  ~ConsistencyCookieBrowserTest() override {}

  // Updates the cookie through the consistency cookie manager and blocks until
  // it completes.
  void SetCookieValue(const std::string& cookie_value) {
    consistency_cookie_manager_->SetValue(cookie_value);
  }

  // Checks the cookie both in HTTP and javascript.
  void CheckCookieValue(const std::string& expected_cookie) {
    // Check that the cookie is set in HTTP.
    ui_test_utils::NavigateToURL(
        browser(),
        https_server_.GetURL(GaiaUrls::GetInstance()->gaia_url().host(),
                             "/echoheader?Cookie"));
    std::string http_cookie =
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "document.body.innerText")
            .ExtractString();
    EXPECT_EQ(expected_cookie, http_cookie);

    // Check that the cookie is available in javascript.
    std::string javascript_cookie =
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "document.cookie")
            .ExtractString();
    EXPECT_EQ(expected_cookie, javascript_cookie);
  }

 private:
  // InProcessBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const GURL& base_url = https_server_.base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    https_server_.StartAcceptingConnections();

    // Setup the CookieConsistencyCookieManager
    Profile* profile = browser()->profile();
    AccountReconcilor* reconcilor =
        AccountReconcilorFactory::GetForProfile(profile);
    std::unique_ptr<TestConsistencyCookieManager> consistency_cookie_manager =
        std::make_unique<TestConsistencyCookieManager>(
            ChromeSigninClientFactory::GetForProfile(profile), reconcilor);
    consistency_cookie_manager_ = consistency_cookie_manager.get();
    reconcilor->SetConsistencyCookieManager(
        std::move(consistency_cookie_manager));
  }

  net::EmbeddedTestServer https_server_;
  TestConsistencyCookieManager* consistency_cookie_manager_;
};

// Tests that the ConsistencyCookieManager can set and change the cookie in HTTP
// and javascript.
IN_PROC_BROWSER_TEST_F(ConsistencyCookieBrowserTest, Basic) {
  // Check the initial value.
  CheckCookieValue(std::string(kConsistencyCookieName) + "=initial_value");
  // Change the cookie.
  SetCookieValue("new_value");
  CheckCookieValue(std::string(kConsistencyCookieName) + "=new_value");
}
