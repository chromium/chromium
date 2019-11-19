// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_USER_SESSION_MANAGER_TEST_API_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_USER_SESSION_MANAGER_TEST_API_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"

namespace chromeos {
namespace test {

// Accesses private data from a UserSessionManager for testing.
class UserSessionManagerTestApi {
 public:
  explicit UserSessionManagerTestApi(UserSessionManager* session_manager);

  // Injects |user_context| that will be used to create StubAuthenticator
  // instance when UserSessionManager::CreateAuthenticator() is called.
  // DEPRECATED: Use InjectStubAuthenticatorBuilder instead.
  void InjectStubUserContext(const UserContext& user_context);

  void InjectAuthenticatorBuilder(
      std::unique_ptr<StubAuthenticatorBuilder> builder);

  // Controls whether browser instance should be launched after sign in
  // (used in tests).
  void SetShouldLaunchBrowserInTests(bool should_launch_browser);

  // Controls whether token handle fetching is enabled (used in tests).
  void SetShouldObtainTokenHandleInTests(bool should_obtain_handle);

  // Sets the function which is used to request a chrome restart.
  void SetAttemptRestartClosureInTests(
      const base::RepeatingClosure& attempt_restart_closure);

 private:
  UserSessionManager* session_manager_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(UserSessionManagerTestApi);
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_USER_SESSION_MANAGER_TEST_API_H_
