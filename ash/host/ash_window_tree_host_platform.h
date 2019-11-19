// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/transformer_helper.h"
#include "ui/aura/window_tree_host_platform.h"

namespace ui {
struct PlatformWindowInitProperties;
}

namespace ash {
class ExtendedMouseWarpControllerTest;

class ASH_EXPORT AshWindowTreeHostPlatform
    : public AshWindowTreeHost,
      public aura::WindowTreeHostPlatform {
 public:
  explicit AshWindowTreeHostPlatform(
      ui::PlatformWindowInitProperties properties);

  ~AshWindowTreeHostPlatform() override;

 protected:
  friend ExtendedMouseWarpControllerTest;
  FRIEND_TEST_ALL_PREFIXES(ExtendedMouseWarpControllerTest,
                           CheckHostPointToScreenInMouseWarpRegion);

  AshWindowTreeHostPlatform();

  // AshWindowTreeHost:
  void ConfineCursorToRootWindow() override;
  void ConfineCursorToBoundsInRoot(const gfx::Rect& bounds_in_root) override;
  gfx::Rect GetLastCursorConfineBoundsInPixels() const override;
  void SetRootWindowTransformer(
      std::unique_ptr<RootWindowTransformer> transformer) override;
  gfx::Insets GetHostInsets() const override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void PrepareForShutdown() override;
  void SetCursorConfig(const display::Display& display,
                       display::Display::Rotation rotation) override;
  void ClearCursorConfig() override;

  // aura::WindowTreeHostPlatform:
  void SetRootTransform(const gfx::Transform& transform) override;
  gfx::Transform GetRootTransform() const override;
  gfx::Transform GetInverseRootTransform() const override;
  gfx::Rect GetTransformedRootWindowBoundsInPixels(
      const gfx::Size& host_size_in_pixels) const override;
  void OnCursorVisibilityChangedNative(bool show) override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  void DispatchEvent(ui::Event* event) override;

 private:
  // All constructors call into this.
  void CommonInit();

  // Temporarily disable the tap-to-click feature. Used on CrOS.
  void SetTapToClickPaused(bool state);

  TransformerHelper transformer_helper_;

  gfx::Rect last_cursor_confine_bounds_in_pixels_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostPlatform);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
