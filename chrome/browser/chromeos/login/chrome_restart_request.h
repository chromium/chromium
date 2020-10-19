// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CHROME_RESTART_REQUEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CHROME_RESTART_REQUEST_H_

#include <string>

class GURL;

namespace base {
class CommandLine;
}

namespace chromeos {

// Determines the `command_line` to be used for the OTR process.
void GetOffTheRecordCommandLine(const GURL& start_url,
                                bool is_oobe_completed,
                                const base::CommandLine& base_command_line,
                                base::CommandLine* command_line);

// Request session manager to restart chrome with a new command line.
void RestartChrome(const base::CommandLine& command_line);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_CHROME_RESTART_REQUEST_H_
