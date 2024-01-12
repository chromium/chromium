// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

// Include the appropriate BrowserProcessPlatformPart based on the platform.
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browser_process_platform_part_android.h"  // IWYU pragma: export
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process_platform_part_ash.h"  // IWYU pragma: export
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/browser_process_platform_part_chromeos.h"  // IWYU pragma: export
class BrowserProcessPlatformPart : public BrowserProcessPlatformPartChromeOS {};
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/browser_process_platform_part_mac.h"  // IWYU pragma: export
#else
#include "chrome/browser/browser_process_platform_part_base.h"  // IWYU pragma: export
class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase {};
#endif

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_H_
