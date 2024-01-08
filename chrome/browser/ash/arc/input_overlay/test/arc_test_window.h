// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_ARC_TEST_WINDOW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_ARC_TEST_WINDOW_H_

#include "base/memory/raw_ptr.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_helper.h"

namespace arc::input_overlay {
namespace test {

// ArcTestWindow creates window with exo/surface.
class ArcTestWindow {
 public:
  ArcTestWindow(exo::test::ExoTestHelper* helper,
                aura::Window* root,
                const std::string& package_name,
                const gfx::Rect bounds=gfx::Rect(10, 10, 100, 100));
  ArcTestWindow(const ArcTestWindow&) = delete;
  ArcTestWindow& operator=(const ArcTestWindow&) = delete;
  ~ArcTestWindow();

  aura::Window* GetWindow();
  void SetMinimized();
  // Set bounds in `display`. `bounds` is the local bounds in the display.
  void SetBounds(display::Display& display, gfx::Rect bounds);

 private:
  raw_ptr<exo::Surface, DanglingUntriaged> surface_;
  std::unique_ptr<exo::ClientControlledShellSurface> shell_surface_;
};

}  // namespace test
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_ARC_TEST_WINDOW_H_
