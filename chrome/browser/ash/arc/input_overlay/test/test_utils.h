// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace arc::input_overlay {

class TouchInjector;

// Create ARC window without exo support.
std::unique_ptr<views::Widget> CreateArcWindow(
    aura::Window* root_window,
    const gfx::Rect& bounds = gfx::Rect(10, 10, 100, 100),
    const std::string& package_name = std::string("arc.packagename"));

// Check the actions size in `injector`, action types, and action IDs.
void CheckActions(TouchInjector* injector,
                  size_t expect_size,
                  const std::vector<ActionType>& expect_types,
                  const std::vector<int>& expect_ids);

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_TEST_UTILS_H_
