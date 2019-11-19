// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SESSION_FLAGS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SESSION_FLAGS_MANAGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"

namespace base {
class CommandLine;
class Value;
}  // namespace base

namespace chromeos {
namespace test {

// Test helper that sets up command line for login tests. By default, it
// initializes the command line so tests start on the login manager.
// It supports session restore mode, which can be used during multi step browser
// tests (i.e. tests that have PRE_ test) to keep session state across test
// runs. If a user session was active in the previous run, this will set up
// command line to restore session for that user, which can be useful for
// testing chrome restart to apply per-session flags, or session restore on
// crash. Additionally, it respects restart job arguments if one was requested
// (restart job is used to restart session as guest user).
class SessionFlagsManager {
 public:
  // Pair of switch name and value. The value can be empty.
  using Switch = std::pair<std::string, std::string>;

  SessionFlagsManager();
  ~SessionFlagsManager();

  // Sets the manager up for session restore.It should be called early in
  // test setup, before calling AppendSwitchesToCommandLine().
  //
  // When session restore is enabled, SessionStateManager will load session
  // state from a file in the test user data directory, and use it to initialize
  // the test command line. The file will contain session information saved
  // during the previous (PRE_) browser test step. The information includes:
  // *   the active user information
  // *   the active user's per-session flags
  // *   restart job flags, if restart job was requested.
  //
  // If the backing file is not found, or empty, command line will be
  // set up with login manager flags so test starts on the login screen.
  void SetUpSessionRestore();

  // Sets the default login policy switches, that will be added to command line
  // when initializing it for login screen.
  // This should be called before chrome startup starts - i.e. before
  // SetUpInProcessBrowserTestFixture().
  void SetDefaultLoginSwitches(const std::vector<Switch>& switches);

  // Sets up the test command line. If session restore is enabled, and
  // persisted session state is found, the command line is set up according to
  // that state. Otherwise, it's set up to force login screen.
  //
  // NOTE: If SetUpSessionRestore() was called, calling this requires the
  // user data dir to be set up for test.
  void AppendSwitchesToCommandLine(base::CommandLine* command_line);

  // Finalizes this instance - if session restore is enabled, it will save
  // current session manager information to a backing file in the user data
  // directory.
  void Finalize();

 private:
  enum class Mode {
    // Sets up command line to start on login screen.
    LOGIN_SCREEN,
    // If saved user session state exists, sets up command line to continue the
    // user session, otherwise sets up command line for login screen.
    LOGIN_SCREEN_WITH_SESSION_RESTORE
  };

  void LoadStateFromBackingFile();
  void StoreStateToBackingFile();
  base::Value GetSwitchesValueFromArgv(const std::vector<std::string>& argv);

  // The mode this manager is running in.
  Mode mode_ = Mode::LOGIN_SCREEN;

  // Whether Finalize has been called.
  bool finalized_ = false;

  // List of default switches that should be added to command line when setting
  // up command line for login screen.
  std::vector<Switch> default_switches_;

  // If command line should be set up for a user session (e.g. when running in
  // session restore mode), the logged in user information.
  std::string user_id_;
  std::string user_hash_;
  base::Optional<std::vector<Switch>> user_flags_;

  // List of switches passed as a restart job arguments.
  base::Optional<std::vector<Switch>> restart_job_;

  // If |session_restore_enabled_| is set, the path to the file where session
  // state is saved.
  base::FilePath backing_file_;

  DISALLOW_COPY_AND_ASSIGN(SessionFlagsManager);
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SESSION_FLAGS_MANAGER_H_
