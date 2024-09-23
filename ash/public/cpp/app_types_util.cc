// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_types_util.h"

#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

namespace ash {

bool IsArcWindow(const aura::Window* window) {
  return window && window->GetProperty(chromeos::kAppTypeKey) ==
                       chromeos::AppType::ARC_APP;
}

bool IsLacrosWindow(const aura::Window* window) {
  return window->GetProperty(chromeos::kAppTypeKey) ==
         chromeos::AppType::LACROS;
}

}  // namespace ash
