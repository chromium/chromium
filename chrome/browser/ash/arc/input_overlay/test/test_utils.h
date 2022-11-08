// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_TEST_UTILS_H_

#include <memory>

#include "ui/aura/window.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

// Create ARC window without exo support.
std::unique_ptr<views::Widget> CreateArcWindow(
    aura::Window* root_window,
    const gfx::Rect& bounds = gfx::Rect(10, 10, 100, 100),
    const std::string& package_name = std::string("arc.packagename"));

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_TEST_UTILS_H_
