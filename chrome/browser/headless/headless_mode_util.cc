// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_util.h"

#include "build/build_config.h"

// New headless mode is available on Linux, Windows and Mac platforms.
// More platforms will be added later, so avoid function level clutter
// by providing stub implementations at the end of the file.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#include "base/check_deref.h"
#include "base/command_line.h"
#include "chrome/browser/headless/headless_mode_switches.h"
#include "ui/gfx/switches.h"

namespace headless {

bool IsHeadlessMode() {
  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
  return command_line.HasSwitch(switches::kHeadless);
}

bool IsChromeSchemeUrlAllowed() {
  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
  return command_line.HasSwitch(switches::kAllowChromeSchemeUrl);
}

}  // namespace headless

#else  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace headless {

bool IsHeadlessMode() {
  return false;
}

bool IsChromeSchemeUrlAllowed() {
  return false;
}

}  // namespace headless

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
