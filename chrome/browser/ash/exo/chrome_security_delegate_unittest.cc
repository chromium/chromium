// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/exo/chrome_security_delegate.h"

#include "ash/constants/app_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

using ChromeSecurityDelegateTest = testing::Test;

TEST_F(ChromeSecurityDelegateTest, CanLockPointer) {
  auto security_delegate = std::make_unique<ChromeSecurityDelegate>();
  aura::Window container_window(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  container_window.Init(ui::LAYER_NOT_DRAWN);
  aura::test::TestWindowDelegate delegate;

  // CanLockPointer should be allowed for arc and lacros, but not others.
  aura::Window* arc_toplevel = aura::test::CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(), &container_window);
  arc_toplevel->SetProperty(aura::client::kAppType,
                            static_cast<int>(AppType::ARC_APP));
  EXPECT_TRUE(security_delegate->CanLockPointer(arc_toplevel));

  aura::Window* lacros_toplevel = aura::test::CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(), &container_window);
  lacros_toplevel->SetProperty(aura::client::kAppType,
                               static_cast<int>(AppType::LACROS));
  EXPECT_TRUE(security_delegate->CanLockPointer(lacros_toplevel));

  aura::Window* crostini_toplevel = aura::test::CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(), &container_window);
  crostini_toplevel->SetProperty(aura::client::kAppType,
                                 static_cast<int>(AppType::CROSTINI_APP));
  EXPECT_FALSE(security_delegate->CanLockPointer(crostini_toplevel));
}

}  // namespace ash
