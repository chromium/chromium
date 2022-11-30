// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTIVE_USE_UTIL_H_
#define CHROME_BROWSER_ACTIVE_USE_UTIL_H_

namespace base {
class CommandLine;
}

// Returns true if a process started with |command_line| should record active
// use in the registry for consumption by Omaha. Unconditionally returns false
// for Windows build configurations that do not integrate with Omaha.
bool ShouldRecordActiveUse(const base::CommandLine& command_line);

#endif  // CHROME_BROWSER_ACTIVE_USE_UTIL_H_
