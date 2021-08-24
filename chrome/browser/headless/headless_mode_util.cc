// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_util.h"

#include "build/build_config.h"

// Native headless is currently available only on Linux platform. More
// platforms will be added soon, so avoid function level clutter by providing
// stub implementations at the end of the file.
#if defined(OS_LINUX)

#include <cstdlib>
#include <vector>

#include "base/base_switches.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace headless {

namespace {

// Chrome native headless mode is enabled by adding the 'chrome' value
// to --headless command line switch or by setting the USE_HEADLESS_CHROME
// environment variable.
const char kChrome[] = "chrome";
const char kUseHeadlessChrome[] = "USE_HEADLESS_CHROME";

bool ParseWindowSize(const std::string& str, int* width, int* height) {
  std::vector<base::StringPiece> width_and_height = base::SplitStringPiece(
      str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (width_and_height.size() != 2)
    return false;

  if (!base::StringToInt(width_and_height[0], width) ||
      !base::StringToInt(width_and_height[1], height))
    return false;

  return true;
}

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
  // If there is no window size specification, provide the default one.
  // This is necessary because ozone/headless defaults to 1x1 desktop
  // causing problems during window size adjustment.
  if (!command_line->HasSwitch(switches::kWindowSize)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWindowSize, "800,600");
  }
  // Enable unattended mode.
  if (!command_line->HasSwitch(::switches::kNoErrorDialogs)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kNoErrorDialogs);
  }
  // Native headless chrome relies on ozone/headless platform.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kEnableFeatures, features::kUseOzonePlatform.name);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kOzonePlatform, switches::kHeadless);
}

void SetHeadlessDisplayBounds() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kWindowSize))
    return;

  int width, height;
  std::string window_size =
      command_line.GetSwitchValueASCII(switches::kWindowSize);
  if (ParseWindowSize(window_size, &width, &height)) {
    display::Screen* screen = display::Screen::GetScreen();
    screen->GetPrimaryDisplay().set_bounds(gfx::Rect(width, height));
  }
}

}  // namespace headless

#else  // defined(OS_LINUX)

namespace headless {

bool IsChromeNativeHeadless() {
  return false;
}

void SetUpCommandLine(const base::CommandLine* command_line) {}

void SetHeadlessDisplayBounds() {}

}  // namespace headless

#endif  // defined(OS_LINUX)
