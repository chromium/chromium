// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/transformer_helper.h"
#include "services/ws/host_event_dispatcher.h"
#include "ui/aura/mus/input_method_mus_delegate.h"
#include "ui/aura/window_tree_host_platform.h"

namespace aura {
class InputMethodMus;
}

namespace ws {
class HostEventQueue;
}

namespace ui {
struct PlatformWindowInitProperties;
}

namespace ash {

class ASH_EXPORT AshWindowTreeHostPlatform
    : public AshWindowTreeHost,
      public aura::WindowTreeHostPlatform,
      public aura::InputMethodMusDelegate,
      public ws::HostEventDispatcher {
 public:
  explicit AshWindowTreeHostPlatform(
      ui::PlatformWindowInitProperties properties);

  ~AshWindowTreeHostPlatform() override;

 protected:
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
  void UpdateTextInputState(ui::mojom::TextInputStatePtr state) override;
  void UpdateImeVisibility(bool visible,
                           ui::mojom::TextInputStatePtr state) override;

  // aura::WindowTreeHostPlatform:
  void SetRootTransform(const gfx::Transform& transform) override;
  gfx::Transform GetRootTransform() const override;
  gfx::Transform GetInverseRootTransform() const override;
  gfx::Rect GetTransformedRootWindowBoundsInPixels(
      const gfx::Size& host_size_in_pixels) const override;
  void OnCursorVisibilityChangedNative(bool show) override;
  void SetBoundsInPixels(const gfx::Rect& bounds,
                         const viz::LocalSurfaceId& local_surface_id,
                         base::TimeTicks allocation_time) override;
  void DispatchEvent(ui::Event* event) override;
  bool ShouldSendKeyEventToIme() override;

  // aura::InputMethodMusDelegate:
  void SetTextInputState(ui::mojom::TextInputStatePtr state) override;
  void SetImeVisibility(bool visible,
                        ui::mojom::TextInputStatePtr state) override;

  // ws::HostEventDispatcher:
  void DispatchEventFromQueue(ui::Event* event) override;

 private:
  // All constructors call into this.
  void CommonInit();

  // Temporarily disable the tap-to-click feature. Used on CrOS.
  void SetTapToClickPaused(bool state);

  TransformerHelper transformer_helper_;

  gfx::Rect last_cursor_confine_bounds_in_pixels_;

  // Use InputMethodMus as the InputMethod implementation. InputMethodMus ends
  // up connection to the UI Service over mojo, which is in process, but
  // simplifies things. In particular, even though the WindowService is in
  // process, parts of ime live in it's own process, so by using InputMethodMus
  // those connections are correctly established.
  std::unique_ptr<aura::InputMethodMus> input_method_;

  std::unique_ptr<ws::HostEventQueue> host_event_queue_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostPlatform);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_PLATFORM_H_
