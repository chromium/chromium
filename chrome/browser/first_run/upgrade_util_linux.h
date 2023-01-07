// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_LINUX_H_
#define CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_LINUX_H_

namespace upgrade_util {

// Saves the last modified time of the chrome executable file.
void SaveLastModifiedTimeOfExe();

// Returns the last modified time of the chrome executable file.
double GetLastModifiedTimeOfExe();

}  // namespace upgrade_util

#endif  // CHROME_BROWSER_FIRST_RUN_UPGRADE_UTIL_LINUX_H_
