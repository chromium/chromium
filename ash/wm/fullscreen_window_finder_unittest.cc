// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/fullscreen_window_finder.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class FullscreenWindowFinderTest : public AshTestBase {
 public:
  FullscreenWindowFinderTest() = default;

  FullscreenWindowFinderTest(const FullscreenWindowFinderTest&) = delete;
  FullscreenWindowFinderTest& operator=(const FullscreenWindowFinderTest&) =
      delete;

  ~FullscreenWindowFinderTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    gfx::Rect bounds(100, 100, 200, 200);
    test_window_.reset(CreateTestWindowInShellWithBounds(bounds));
  }

  void TearDown() override {
    test_window_.reset();
    AshTestBase::TearDown();
  }

  bool FullscreenWindowExists() const {
    return nullptr != GetWindowForFullscreenModeForContext(test_window_.get());
  }

 protected:
  std::unique_ptr<aura::Window> test_window_;
};

// Test that a non-fullscreen window isn't found by GetWindowForFullscreenMode.
TEST_F(FullscreenWindowFinderTest, NonFullscreen) {
  EXPECT_FALSE(FullscreenWindowExists());
}

// Test that a regular fullscreen window is found by GetWindowForFullscreenMode.
TEST_F(FullscreenWindowFinderTest, RegularFullscreen) {
  test_window_->SetProperty(aura::client::kShowStateKey,
                            ui::mojom::WindowShowState::kFullscreen);
  EXPECT_TRUE(FullscreenWindowExists());
}

// Test that a pinned fullscreen window is found by GetWindowForFullscreenMode.
TEST_F(FullscreenWindowFinderTest, PinnedFullscreen) {
  window_util::PinWindow(test_window_.get(), /*trusted=*/false);
  EXPECT_TRUE(FullscreenWindowExists());
}

// Test that a trusted pinned fullscreen window is found by
// GetWindowForFullscreenMode.
TEST_F(FullscreenWindowFinderTest, TrustedPinnedFullscreen) {
  window_util::PinWindow(test_window_.get(), /*trusted=*/true);
  EXPECT_TRUE(FullscreenWindowExists());
}

}  // namespace ash
