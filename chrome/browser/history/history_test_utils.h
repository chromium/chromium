// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_TEST_UTILS_H_
#define CHROME_BROWSER_HISTORY_HISTORY_TEST_UTILS_H_

class Profile;

// Spins the current message loop until all pending messages on the history DB
// thread complete.
void WaitForHistoryBackendToRun(Profile* profile);

#endif  // CHROME_BROWSER_HISTORY_HISTORY_TEST_UTILS_H_
