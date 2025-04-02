// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_linux.h"

#include <optional>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "ui/linux/display_server_utils.h"

namespace {

// Do not change the values of these entries since they're recorded in UMA.
enum class DisplayServerSupport {
  // Chrome will fail to launch without a display server.
  kNone = 0,
  kX11 = 1,
  // The primary display server is Wayland, but X11 is provided via XWayland.
  kXWayland = 2,
  kWaylandOnly = 3,

  kMaxValue = kWaylandOnly,
};

DisplayServerSupport GetDisplayServerSupport(bool x11, bool wayland) {
  if (x11 && wayland) {
    return DisplayServerSupport::kXWayland;
  }
  if (x11) {
    return DisplayServerSupport::kX11;
  }
  if (wayland) {
    return DisplayServerSupport::kWaylandOnly;
  }
  return DisplayServerSupport::kNone;
}

void RecordDisplayServerProtocolSupport() {
  auto env = base::Environment::Create();
  base::UmaHistogramEnumeration(
      "Linux.DisplayServerSupport",
      GetDisplayServerSupport(ui::HasX11Display(*env),
                              ui::HasWaylandDisplay(*env)));
}

}  // namespace

ChromeBrowserMainExtraPartsLinux::ChromeBrowserMainExtraPartsLinux() = default;

ChromeBrowserMainExtraPartsLinux::~ChromeBrowserMainExtraPartsLinux() = default;

void ChromeBrowserMainExtraPartsLinux::PostBrowserStart() {
  RecordDisplayServerProtocolSupport();
  ChromeBrowserMainExtraPartsOzone::PostBrowserStart();
}

// static
void ChromeBrowserMainExtraPartsLinux::InitOzonePlatformHint() {
#if BUILDFLAG(IS_LINUX)
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::optional<std::string> desktop_startup_id =
      env->GetVar("DESKTOP_STARTUP_ID");
  if (desktop_startup_id.has_value()) {
    command_line->AppendSwitchASCII("desktop-startup-id",
                                    *desktop_startup_id);
  }
#endif  // BUILDFLAG(IS_LINUX)
}
