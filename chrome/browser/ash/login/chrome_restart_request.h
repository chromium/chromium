// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_CHROME_RESTART_REQUEST_H_
#define CHROME_BROWSER_ASH_LOGIN_CHROME_RESTART_REQUEST_H_

#include <stdint.h>

class GURL;
class PrefService;

namespace base {
class CommandLine;
}

namespace ash {

// Keep in sync with RestartJobReason in
// chromeos/ash/components/dbus/session_manager/session_manager_client.h
enum class RestartChromeReason : uint32_t {
  // Restart browser for Guest session.
  kGuest = 0,
  // Restart browser without user session for headless Chromium.
  kUserless = 1,
};

// Determines the `command_line` to be used for the OTR process.
void GetOffTheRecordCommandLine(const GURL& start_url,
                                const base::CommandLine& base_command_line,
                                base::CommandLine* command_line);

// Request session manager to restart chrome with a new command line.
// The request is sent once the pending writes of `local_state` are committed,
// or after a timeout. `local_state` is only used during this call.
// |reason| - reason to restart chrome with user session (for guest sessions
// only) or without user session (for headless chrome).
void RestartChrome(PrefService& local_state,
                   const base::CommandLine& command_line,
                   RestartChromeReason reason);

// RestartChrome() must be called at most once per process. Resets the state so
// that tests can call it again.
void ResetRestartRequestedForTesting();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_CHROME_RESTART_REQUEST_H_
