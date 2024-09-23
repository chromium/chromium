// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/oobe_auth_page_waiter.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {
namespace test {
namespace {

constexpr char kGaiaAuthenticator[] = "$('gaia-signin').authenticator_";
constexpr char kEnrollmentAuthenticator[] =
    "$('enterprise-enrollment').authenticator_";

void MaybeWaitForOobeToInitialize() {
  base::RunLoop run_loop;
  if (!LoginDisplayHost::default_host()->GetOobeUI()->IsJSReady(
          run_loop.QuitClosure())) {
    run_loop.Run();
  }
}

}  // namespace

OobeAuthPageWaiter::OobeAuthPageWaiter(AuthPageType auth_page_type)
    : auth_page_type_(auth_page_type) {}

OobeAuthPageWaiter::~OobeAuthPageWaiter() = default;

void OobeAuthPageWaiter::WaitUntilReady() {
  WaitForEvent("ready");
}

void OobeAuthPageWaiter::WaitForEvent(const std::string& event) {
  // Starts listening to message before executing the JS code that generates
  // the message below.
  std::string js =
      R"((function() {
              var authenticator = $AuthenticatorId;
              var f = function() {
                authenticator.removeEventListener('$Event', f);
                window.domAutomationController.send('Done');
              };
              authenticator.addEventListener('$Event', f);
            })();)";
  base::ReplaceSubstringsAfterOffset(&js, 0, "$AuthenticatorId",
                                     GetAuthenticator());
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Event", event);

  // OOBE UI must be initialized for the JS to execute safely. Otherwise
  // the call might hang or won't execute properly.
  MaybeWaitForOobeToInitialize();

  content::DOMMessageQueue message_queue(
      LoginDisplayHost::default_host()->GetOobeWebContents());
  OobeJS().Evaluate(js);

  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"Done\"");
}

const char* OobeAuthPageWaiter::GetAuthenticator() {
  switch (auth_page_type_) {
    case AuthPageType::GAIA:
      return kGaiaAuthenticator;
    case AuthPageType::ENROLLMENT:
      return kEnrollmentAuthenticator;
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected value for auth_page_type_";
  return kGaiaAuthenticator;
}

OobeAuthPageWaiter OobeGaiaPageWaiter() {
  return OobeAuthPageWaiter(OobeAuthPageWaiter::AuthPageType::GAIA);
}

OobeAuthPageWaiter OobeEnrollmentPageWaiter() {
  return OobeAuthPageWaiter(OobeAuthPageWaiter::AuthPageType::ENROLLMENT);
}

}  // namespace test
}  // namespace ash
