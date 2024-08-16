// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_IMPL_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_IMPL_H_

#include "ash/public/cpp/external_arc/overlay/arc_overlay_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view_observer.h"

namespace ash {

// Maintains an exo::ShellSurface as an overlay on top of another Aura window.
//
// An instance of this class is first constructed for the host window, which the
// overlay window will appear on top of. The overlay window is then attached via
// a call to |AttachOverlay| as it is expected to be created after the host.
class ASH_PUBLIC_EXPORT ArcOverlayControllerImpl : public ArcOverlayController,
                                                   public aura::WindowObserver,
                                                   public views::ViewObserver {
 public:
  explicit ArcOverlayControllerImpl(aura::Window* host_window);
  ~ArcOverlayControllerImpl() override;

  // Disallow copying and moving
  // Note: Moving an observer would require some work to deregister/re-register
  // it. If the instance is owned by a unique_ptr, the pointer can be moved much
  // more cheaply.
  ArcOverlayControllerImpl(const ArcOverlayControllerImpl&) = delete;
  ArcOverlayControllerImpl(ArcOverlayControllerImpl&&) = delete;
  ArcOverlayControllerImpl& operator=(const ArcOverlayControllerImpl&) = delete;
  ArcOverlayControllerImpl& operator=(ArcOverlayControllerImpl&&) = delete;

  // ArcOverlayController:
  void AttachOverlay(aura::Window* overlay_window) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

  views::NativeViewHost* overlay_container_for_test() {
    return overlay_container_;
  }

 private:
  void UpdateHostBounds();
  void ConvertPointFromWindow(aura::Window* window, gfx::Point* point);
  void EnsureOverlayWindowClosed();
  void OnOverlayWindowClosed();
  void ResetFocusBehavior();
  void RestoreHostCanConsumeSystemKeys();

  raw_ptr<aura::Window> host_window_ = nullptr;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      host_window_observer_{this};

  raw_ptr<aura::Window> overlay_window_ = nullptr;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      overlay_window_observer_{this};

  raw_ptr<views::NativeViewHost> overlay_container_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      overlay_container_observer_{this};

  bool saved_host_can_consume_system_keys_ = false;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_IMPL_H_
