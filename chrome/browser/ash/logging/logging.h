// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGGING_LOGGING_H_
#define CHROME_BROWSER_ASH_LOGGING_LOGGING_H_

namespace base {
class CommandLine;
}

namespace ash {

// Redirects chrome logging to the appropriate session log dir.
void RedirectChromeLogging(const base::CommandLine& command_line);

// Forces log redirection to occur, even if not running on ChromeOS hardware.
void ForceLogRedirectionForTesting();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGGING_LOGGING_H_
