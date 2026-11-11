// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/chrome_browser_main_extra_parts_headless.h"

#include "build/build_config.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "components/headless/clipboard/headless_clipboard.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/events/devices/device_data_manager.h"            // nogncheck
#include "ui/events/devices/device_hotplug_event_observer.h"  // nogncheck
#include "ui/events/devices/input_device.h"                   // nogncheck
#endif

namespace headless {

ChromeBrowserMainExtraPartsHeadless::ChromeBrowserMainExtraPartsHeadless() =
    default;
ChromeBrowserMainExtraPartsHeadless::~ChromeBrowserMainExtraPartsHeadless() =
    default;

void ChromeBrowserMainExtraPartsHeadless::PreBrowserStart() {
  if (!IsHeadlessMode()) {
    return;
  }

  // Headless mode uses platform independent clipboard which we need to set
  // before generic clipboard implementation sets the platform specific one.
  SetHeadlessClipboardForCurrentThread();

#if BUILDFLAG(IS_LINUX)
  // Ensure headless Linux reports a fine pointer so that interaction media
  // features match other desktop platforms.
  ui::DeviceHotplugEventObserver* observer =
      ui::DeviceDataManager::GetInstance();
  observer->OnMouseDevicesUpdated({ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Headless Mouse")});
  observer->OnDeviceListsComplete();
#endif
}

}  // namespace headless
