// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_WINDOW_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_WINDOW_HANDLER_H_

#include "components/services/app_service/public/mojom/types.mojom.h"

namespace chromeos {
namespace full_restore {

// Returns true if the ARC supports ghost window.
bool IsArcGhostWindowEnabled();

// Converts window bounds from Chrome DP to ARC pixels units, and adjust
// window position on display.
apps::mojom::WindowInfoPtr ConvertToArcBounds(
    int64_t display_id,
    apps::mojom::WindowInfoPtr window_info);

// The ArcWindowHandler class provides control for ARC ghost window.
class ArcWindowHandler {
 public:
  ArcWindowHandler() = default;
  ArcWindowHandler(const ArcWindowHandler&) = delete;
  ArcWindowHandler& operator=(const ArcWindowHandler&) = delete;
  ~ArcWindowHandler() = default;
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_ARC_WINDOW_HANDLER_H_
