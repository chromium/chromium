// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains utility functions for lacros_chrome_browsertests.

#ifndef CHROME_BROWSER_LACROS_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_LACROS_BROWSER_TEST_UTIL_H_

#include <stdint.h>

class Browser;

// Some crosapi or Wayland methods rely on assumptions in exo about Window
// focus, or other window attributes. Since Wayland is an async protocol, we
// sometimes need to first wait for exo/Wayland to be aware of the Lacros
// windows before proceeding. We use the Snapshot crosapi as a convenient
// mechanism for doing so.
//
// |browser| is a browser instance that will be navigated to a fixed URL.
// Returns the window ID from the snapshot crosapi.
uint64_t WaitForLacrosToBeAvailableInAsh(Browser* browser);

#endif  // CHROME_BROWSER_LACROS_BROWSER_TEST_UTIL_H_
