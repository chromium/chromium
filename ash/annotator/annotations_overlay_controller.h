// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ANNOTATOR_ANNOTATIONS_OVERLAY_CONTROLLER_H_
#define ASH_ANNOTATOR_ANNOTATIONS_OVERLAY_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
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
class ASH_EXPORT AnnotationsOverlayController {
 public:
  // Constructs the overlay widget, and adds it as a child of `window`.
  // `initial_bounds_in_parent` are initial region bounds selected by the user.
  // Set only if the surface for annotating is a region.
  AnnotationsOverlayController(aura::Window* window,
                             const gfx::Rect& initial_bounds_in_parent);
  AnnotationsOverlayController(const AnnotationsOverlayController&) = delete;
  AnnotationsOverlayController& operator=(const AnnotationsOverlayController&) =
      delete;
  ~AnnotationsOverlayController() = default;

  bool is_enabled() const { return is_enabled_; }

  // Toggles the overlay on or off.
  void Toggle();

  // Sets the bounds of the overlay widget. The given bounds should be relative
  // to the parent `window`.
  void SetBounds(const gfx::Rect& bounds_in_parent);

  // Gets the underlying window of `overlay_widget_`.
  aura::Window* GetOverlayNativeWindow();

 private:
  // Starts or stops showing the overlay on top of the surface.
  void Start();
  void Stop();

  // Updates the z-order of the `overlay_widget_`'s native window.
  void UpdateWidgetStacking();

  // The overlay widget and its contents view.
  views::UniqueWidgetPtr overlay_widget_ = std::make_unique<views::Widget>();
  raw_ptr<AnnotationsOverlayView> annotations_overlay_view_ = nullptr;

  // Whether the overlay is currently enabled and showing on top of the recorded
  // surface.
  bool is_enabled_ = false;
};

}  // namespace ash

#endif  // ASH_ANNOTATOR_ANNOTATIONS_OVERLAY_CONTROLLER_H_
