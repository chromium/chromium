// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ANNOTATOR_ANNOTATIONS_OVERLAY_CONTROLLER_H_
#define ASH_ANNOTATOR_ANNOTATIONS_OVERLAY_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx{
class Rect;
}

namespace ash {

class AnnotationsOverlayView;

// Constructs and owns the widget that will be used as an overlay on top of the
// underlying surface to host annotations.
class ASH_EXPORT AnnotationsOverlayController
    : public aura::WindowObserver,
      public display::DisplayObserver {
 public:
  // Constructs the overlay widget, and adds it as a child of `window`.
  // `partial_region_bounds` are initial region bounds selected by the user. Set
  // only if the surface for annotating is a region.
  AnnotationsOverlayController(aura::Window* window,
                               std::optional<gfx::Rect> partial_region_bounds);
  AnnotationsOverlayController(const AnnotationsOverlayController&) = delete;
  AnnotationsOverlayController& operator=(const AnnotationsOverlayController&) =
      delete;
  ~AnnotationsOverlayController() override;

  bool is_enabled() const { return is_enabled_; }

  // Toggles the overlay on or off.
  void Toggle();

  // Gets the underlying window of `overlay_widget_`.
  aura::Window* GetOverlayNativeWindow();

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

 private:
  // Starts or stops showing the overlay on top of the surface.
  void Start();
  void Stop();

  // Updates the z-order of the `overlay_widget_`'s native window.
  void UpdateWidgetStacking();

  // Sets the bounds of the overlay widget. The given bounds should be relative
  // to the parent `window`.
  void SetBounds(const gfx::Rect& bounds_in_parent);

  // Returns the bounds that should be used for the annotations overlay widget
  // relative to its parent `window_`.
  gfx::Rect GetOverlayWidgetBounds() const;

  // Resets the observations and raw pointers.
  void Reset();

  // The overlay widget and its contents view.
  views::UniqueWidgetPtr overlay_widget_ = std::make_unique<views::Widget>();
  raw_ptr<AnnotationsOverlayView> annotations_overlay_view_ = nullptr;

  // Whether the overlay is currently enabled and showing on top of the recorded
  // surface.
  bool is_enabled_ = false;

  // Window on top of which the overlay is shown.
  raw_ptr<aura::Window> window_ = nullptr;

  // User-selected region bounds for video recording. This field must be
  // provided and cannot be empty if in region video recording mode.
  std::optional<gfx::Rect> partial_region_bounds_;

  base::ScopedObservation<display::Screen, display::DisplayObserver>
      display_observation_{this};
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_ANNOTATOR_ANNOTATIONS_OVERLAY_CONTROLLER_H_
