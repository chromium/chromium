// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_LINUX_H_
#define CHROME_APP_CHROME_MAIN_LINUX_H_

namespace base {
class CommandLine;
}

// Appends additional command line arguments and flags coming from
// CHROME_EXTRA_FLAGS and CHROME_EXTRA_FLAGS_{CHANNEL} environment variables, if
// set, whereas CHANNEL corresponds to the Chrome's release channel or
// Chromium's CHROME_VERSION_EXTRA environment variable.
void AppendExtraArgumentsToCommandLine(base::CommandLine* command_line);

// For Chrome-branded builds, attempts to determine the correct channel to use
// when Chrome is launched outside of the "google-chrome" wrapper script.
void PossiblyDetermineFallbackChromeChannel(const char* launched_binary_path);

#endif  // CHROME_APP_CHROME_MAIN_LINUX_H_
