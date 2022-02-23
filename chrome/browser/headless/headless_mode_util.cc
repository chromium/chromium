// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_util.h"

#include "build/build_config.h"

// Native headless is currently available on Linux, Windows and Mac platforms.
// More platforms will be added later, so avoid function level clutter by
// providing stub implementations at the end of the file.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#include <cstdlib>
#include <vector>

#include "base/base_switches.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/public/ozone_switches.h"
#endif  // BUILDFLAG(IS_LINUX)

namespace headless {

namespace {

// Chrome native headless mode is enabled by adding the 'chrome' value
// to --headless command line switch or by setting the USE_HEADLESS_CHROME
// environment variable.
const char kChrome[] = "chrome";
const char kUseHeadlessChrome[] = "USE_HEADLESS_CHROME";
}  // namespace

bool IsChromeNativeHeadless() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kHeadless)) {
    if (command_line->GetSwitchValueASCII(switches::kHeadless) == kChrome)
      return true;
    if (getenv(kUseHeadlessChrome) != nullptr)
      return true;
  }

  return false;
}

void SetUpCommandLine(const base::CommandLine* command_line) {
  DCHECK(IsChromeNativeHeadless());
  // Enable unattended mode.
  if (!command_line->HasSwitch(::switches::kNoErrorDialogs)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kNoErrorDialogs);
  }
#if BUILDFLAG(IS_LINUX)
  // Native headless chrome on Linux relies on ozone/headless platform.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kOzonePlatform, switches::kHeadless);
  if (!command_line->HasSwitch(switches::kOzoneOverrideScreenSize)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kOzoneOverrideScreenSize, "800,600");
  }
#endif  // BUILDFLAG(IS_LINUX)
}

}  // namespace headless

#else  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace headless {

bool IsChromeNativeHeadless() {
  return false;
}

void SetUpCommandLine(const base::CommandLine* command_line) {}

}  // namespace headless

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
