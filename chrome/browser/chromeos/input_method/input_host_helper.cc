// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_host_helper.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace chromeos {

namespace input_host_helper {

void PopulateInputHost(InputAssociatedHost* host) {
  aura::Window* window = ash::window_util::GetActiveWindow();
  if (window) {
    // TODO(crbug/163645900): Get app_type via aura::client::kAppType.
    const std::string* key = window->GetProperty(ash::kAppIDKey);
    if (key) {
      host->app_key = *key;
    }
  }
}

}  // namespace input_host_helper
}  // namespace chromeos
