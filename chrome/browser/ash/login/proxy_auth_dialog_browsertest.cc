// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/http_auth_dialog/http_auth_dialog.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/register_basic_auth_handler.h"

namespace ash {

// Boolean parameter is used to run this test for webview (true) and for
// iframe (false) GAIA sign in.
class ProxyAuthOnUserBoardScreenTest : public LoginManagerTest {
 public:
  ProxyAuthOnUserBoardScreenTest() {
    login_manager_mixin_.AppendRegularUsers(1);
  }

  ProxyAuthOnUserBoardScreenTest(const ProxyAuthOnUserBoardScreenTest&) =
      delete;
  ProxyAuthOnUserBoardScreenTest& operator=(
      const ProxyAuthOnUserBoardScreenTest&) = delete;

  ~ProxyAuthOnUserBoardScreenTest() override = default;

  void SetUp() override {
    // No need to configure the "proxy" test server to proxy anything. This test
    // only requires the configured "proxy" return proxy auth challenges.
    net::test_server::RegisterProxyBasicAuthHandler(proxy_server_, "user",
                                                    "pass");
    ASSERT_TRUE(proxy_server_.Start());
    LoginManagerTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kProxyServer,
                                    proxy_server_.host_port_pair().ToString());
  }

 private:
  net::test_server::EmbeddedTestServer proxy_server_{
      net::test_server::EmbeddedTestServer::Type::TYPE_HTTP};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// TODO(crbug.com/41486698): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ProxyAuthDialogOnUserBoardScreen \
  DISABLED_ProxyAuthDialogOnUserBoardScreen
#else
#define MAYBE_ProxyAuthDialogOnUserBoardScreen ProxyAuthDialogOnUserBoardScreen
#endif
IN_PROC_BROWSER_TEST_F(ProxyAuthOnUserBoardScreenTest,
                       MAYBE_ProxyAuthDialogOnUserBoardScreen) {
  ASSERT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"user-creation", "selfButton"});
  test::OobeJS().TapOnPath({"user-creation", "nextButton"});
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  ASSERT_TRUE(base::test::RunUntil(
      []() { return HttpAuthDialog::GetAllDialogsForTest().size() == 1; }));
}

}  // namespace ash
