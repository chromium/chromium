// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_AUTH_PAGE_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_AUTH_PAGE_WAITER_H_

#include <string>

namespace ash {
namespace test {

// Utility class for tests that allows waiting for different page events on
// Gaia Login and Enrollment screens.
// TODO(tonydeluna): Remove this functionality from OobeBaseTest and migrate all
// callers to this class.
class OobeAuthPageWaiter {
 public:
  enum class AuthPageType { GAIA, ENROLLMENT };

  // Gaia Login and Enrollment screens use different authenticators.
  // `auth_page_type` specifies which authenticator to wait for.
  explicit OobeAuthPageWaiter(AuthPageType auth_page_type);

  OobeAuthPageWaiter& operator=(const OobeAuthPageWaiter&) = delete;

  ~OobeAuthPageWaiter();

  // Waits for the "ready" event be triggered by the page authenticator.
  void WaitUntilReady();

 private:
  // Waits for `event` to be triggered by the page authenticator.
  void WaitForEvent(const std::string& event);

  const char* GetAuthenticator();

  const AuthPageType auth_page_type_;
};

// Helper that creates an OobeAuthPageWaiter that listens for Gaia page JS
// events.
OobeAuthPageWaiter OobeGaiaPageWaiter();

// Helper that creates an OobeAuthPageWaiter that listens for Enrollment page JS
// events.
OobeAuthPageWaiter OobeEnrollmentPageWaiter();

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_AUTH_PAGE_WAITER_H_
