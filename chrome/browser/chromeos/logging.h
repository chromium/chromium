// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGGING_H_
#define CHROME_BROWSER_CHROMEOS_LOGGING_H_

namespace base {
class CommandLine;
}

namespace logging {

// Redirects chrome logging to the appropriate session log dir.
void RedirectChromeLogging(const base::CommandLine& command_line);

// Forces log redirection to occur, even if not running on ChromeOS hardware.
void ForceLogRedirectionForTesting();

}  // namespace logging

#endif  // CHROME_BROWSER_CHROMEOS_LOGGING_H_
