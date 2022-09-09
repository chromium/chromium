// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_STARTUP_FLAGS_H_
#define CHROME_BROWSER_ANDROID_CHROME_STARTUP_FLAGS_H_

// Force-appends flags to the Chrome command line turning on Android-specific
// features owned by Chrome. This is called as soon as possible during
// initialization to make sure code sees the new flags.
void SetChromeSpecificCommandLineFlags();

#endif  // CHROME_BROWSER_ANDROID_CHROME_STARTUP_FLAGS_H_
