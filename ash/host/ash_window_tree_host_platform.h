// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/transformer_helper.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/ozone/public/input_controller.h"

namespace ui {
struct PlatformWindowInitProperties;
}

namespace ash {
class AshWindowTreeHostDelegate;

class ExtendedMouseWarpControllerTest;
class AshWindowTreeHostPlatformTest;

class ASH_EXPORT AshWindowTreeHostPlatform
    : public AshWindowTreeHost,
      public aura::WindowTreeHostPlatform {
 public:
  AshWindowTreeHostPlatform(ui::PlatformWindowInitProperties properties,
                            AshWindowTreeHostDelegate* delegate);

  AshWindowTreeHostPlatform(const AshWindowTreeHostPlatform&) = delete;
  AshWindowTreeHostPlatform& operator=(const AshWindowTreeHostPlatform&) =
      delete;

  ~AshWindowTreeHostPlatform() override;

  void set_ignore_platform_damage_rect_for_test(bool ignore) {
    ignore_platform_damage_rect_for_test_ = ignore;
  }

 protected:
  friend ExtendedMouseWarpControllerTest;
  FRIEND_TEST_ALL_PREFIXES(ExtendedMouseWarpControllerTest,
                           CheckHostPointToScreenInMouseWarpRegion);
  friend AshWindowTreeHostPlatformTest;
  FRIEND_TEST_ALL_PREFIXES(AshWindowTreeHostPlatformTest, UnadjustedMovement);

  AshWindowTreeHostPlatform(std::unique_ptr<ui::PlatformWindow> window,
                            AshWindowTreeHostDelegate* delegate,
                            size_t compositor_memory_limit_mb = 0);

  // AshWindowTreeHost:
  void ConfineCursorToRootWindow() override;
  void ConfineCursorToBoundsInRoot(const gfx::Rect& bounds_in_root) override;
  gfx::Rect GetLastCursorConfineBoundsInPixels() const override;
  void SetRootWindowTransformer(
      std::unique_ptr<RootWindowTransformer> transformer) override;
  gfx::Insets GetHostInsets() const override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void PrepareForShutdown() override;
  void UpdateCursorConfig() override;
  void ClearCursorConfig() override;
  void UpdateRootWindowSize() override;

  // aura::WindowTreeHostPlatform:
  void SetRootTransform(const gfx::Transform& transform) override;
  gfx::Transform GetRootTransform() const override;
  gfx::Transform GetInverseRootTransform() const override;
  gfx::Rect GetTransformedRootWindowBoundsFromPixelSize(
      const gfx::Size& host_size_in_pixels) const override;
  void OnCursorVisibilityChangedNative(bool show) override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  void OnDamageRect(const gfx::Rect& damage_rect) override;
  void DispatchEvent(ui::Event* event) override;
  std::unique_ptr<aura::ScopedEnableUnadjustedMouseEvents>
  RequestUnadjustedMovement() override;

  raw_ptr<AshWindowTreeHostDelegate, DanglingUntriaged> delegate_ =
      nullptr;  // Not owned.

 private:
  // All constructors call into this.
  void CommonInit();

  // Temporarily disable the tap-to-click feature. Used on CrOS.
  void SetTapToClickPaused(bool state);

  TransformerHelper transformer_helper_;

  raw_ptr<ui::InputController, DanglingUntriaged> input_controller_ = nullptr;

  gfx::Rect last_cursor_confine_bounds_in_pixels_;

  bool ignore_platform_damage_rect_for_test_ = false;
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
