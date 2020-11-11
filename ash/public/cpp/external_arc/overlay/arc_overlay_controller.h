// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
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
class ASH_PUBLIC_EXPORT ArcOverlayController : public aura::WindowObserver,
                                               public views::ViewObserver {
 public:
  explicit ArcOverlayController(aura::Window* host_window);
  ~ArcOverlayController() override;

  // Disallow copying and moving
  // Note: Moving an observer would require some work to deregister/re-register
  // it. If the instance is owned by a unique_ptr, the pointer can be moved much
  // more cheaply.
  ArcOverlayController(const ArcOverlayController&) = delete;
  ArcOverlayController(ArcOverlayController&&) = delete;
  ArcOverlayController& operator=(const ArcOverlayController&) = delete;
  ArcOverlayController& operator=(ArcOverlayController&&) = delete;

  // Attaches the window that is intended to be used as the overlay.
  // This is expected to be a toplevel window, and it will be reparented.
  void AttachOverlay(aura::Window* overlay_window);

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  void UpdateHostBounds();
  void ConvertPointFromWindow(aura::Window* window, gfx::Point* point);

  aura::Window* host_window_ = nullptr;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      host_window_observer_{this};

  aura::Window* overlay_window_ = nullptr;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      overlay_window_observer_{this};

  views::NativeViewHost* overlay_container_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      overlay_container_observer_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_H_
