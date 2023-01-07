// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FULLSCREEN_H_
#define CHROME_BROWSER_FULLSCREEN_H_

#include <stdint.h>

#include "build/build_config.h"

// Safe to call from cross-platform code; implementation is different for each
// platform. Not implemented on Chrome OS.
bool IsFullScreenMode();

#endif  // CHROME_BROWSER_FULLSCREEN_H_
