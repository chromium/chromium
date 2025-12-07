// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SESSION_CHROME_SESSION_MANAGER_H_
#define CHROME_BROWSER_ASH_LOGIN_SESSION_CHROME_SESSION_MANAGER_H_

#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/session/user_session_initializer.h"
#include "chromeos/ash/components/login/integrity/misconfigured_user_cleaner.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace base {
class CommandLine;
}

class Profile;

namespace ash {

class SessionLengthLimiter;

class ChromeSessionManager : public session_manager::SessionManager {
 public:
  ChromeSessionManager(
      std::unique_ptr<session_manager::SessionManagerDelegate> delegate);

  ChromeSessionManager(const ChromeSessionManager&) = delete;
  ChromeSessionManager& operator=(const ChromeSessionManager&) = delete;

  ~ChromeSessionManager() override;

  // Registers session manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Initialize session manager on browser starts up. Runs different code
  // path based on command line flags and policy. Possible scenarios include:
  //   - Launches pre-session UI such as  out-of-box or login;
  //   - Launches the auto launched kiosk app;
  //   - Resumes user sessions on crash-and-restart;
  //   - Starts a stub login session for dev or test;
  void Initialize(const base::CommandLine& parsed_command_line,
                  Profile* profile,
                  bool is_running_test);

  // Shuts down the session manager.
  void Shutdown();

  // session_manager::SessionManager:
  void OnUserManagerCreated(user_manager::UserManager* user_manager) override;
  void SessionStarted() override;
  void OnSessionCreated(bool browser_restart) override;

  // user_manager::UserManager::Observer:
  void OnUsersSignInConstraintsChanged() override;

  SessionLengthLimiter* GetSessionLengthLimiterForTesting() {
    return session_length_limiter_.get();
  }

 private:
  std::unique_ptr<OobeConfiguration> oobe_configuration_;
  std::unique_ptr<UserSessionInitializer> user_session_initializer_;
  std::unique_ptr<SessionLengthLimiter> session_length_limiter_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SESSION_CHROME_SESSION_MANAGER_H_
