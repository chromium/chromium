// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_pin_util.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace {

// TODO(elkurin): Migrate this into chromeos common test with lacros.
class WindowPinUtilTest : public ash::AshTestBase {};

TEST_F(WindowPinUtilTest, IsPinned_Pinned) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  PinWindow(window.get(), /*trusted=*/false);
  EXPECT_TRUE(IsWindowPinned(window.get()));
}

TEST_F(WindowPinUtilTest, IsPinned_TrustedPinned) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  PinWindow(window.get(), /*trusted=*/true);
  EXPECT_TRUE(IsWindowPinned(window.get()));
}

TEST_F(WindowPinUtilTest, IsPinned_Unpinned) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  PinWindow(window.get(), /*trusted=*/true);
  ASSERT_TRUE(IsWindowPinned(window.get()));

  UnpinWindow(window.get());
  EXPECT_FALSE(IsWindowPinned(window.get()));
}

TEST_F(WindowPinUtilTest, IsPinned_FullscreenNotPinned) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  wm::SetWindowFullscreen(window.get(), /*fullscreen=*/true);
  EXPECT_FALSE(IsWindowPinned(window.get()));
}

}  // namespace
