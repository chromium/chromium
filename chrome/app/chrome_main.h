// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_H_
#define CHROME_APP_CHROME_MAIN_H_

namespace base {
class CommandLine;
}

// Returns the command line as it existed immediately after
// initialization in the browser process, before any internal Chromium
// programmatic mutations.
// This function is only available in the browser process.
const base::CommandLine& GetInitialBrowserCommandLine();

#endif  // CHROME_APP_CHROME_MAIN_H_
