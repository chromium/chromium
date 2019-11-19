// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_H_

#include "build/build_config.h"

// Include the appropriate BrowserProcessPlatformPart based on the platform.
#if defined(OS_ANDROID)
#include "chrome/browser/browser_process_platform_part_android.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/browser_process_platform_part_mac.h"
#elif defined(OS_WIN)
#include "chrome/browser/browser_process_platform_part_win.h"
#else
#include "chrome/browser/browser_process_platform_part_base.h"
class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase {};
#endif

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_H_
