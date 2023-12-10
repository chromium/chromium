// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system_tray.h"

#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"

namespace ash {

// static
SystemTray* SystemTray::Get() {
  // Use the `SystemTray` object created inside `Shell` for this global getter.
  // Note that `Shell` can be destroyed before this getter is called.
  return Shell::HasInstance() ? Shell::Get()->system_tray_model() : nullptr;
}

}  // namespace ash
