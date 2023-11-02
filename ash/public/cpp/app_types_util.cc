// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_types_util.h"

#include "ash/constants/app_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {

bool IsArcWindow(const aura::Window* window) {
  return window && window->GetProperty(aura::client::kAppType) ==
                       static_cast<int>(ash::AppType::ARC_APP);
}

bool IsLacrosWindow(const aura::Window* window) {
  return window->GetProperty(aura::client::kAppType) ==
         static_cast<int>(ash::AppType::LACROS);
}

}  // namespace ash
