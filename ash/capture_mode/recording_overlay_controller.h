// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_RECORDING_OVERLAY_CONTROLLER_H_
#define ASH_CAPTURE_MODE_RECORDING_OVERLAY_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class RecordingOverlayView;

// Constructs and owns the widget that will be used as an overlay on top of the
// recorded surface, to host contents, such as the Projector mode's annotations,
// which are meant to be shown as part of the recording.
class ASH_EXPORT RecordingOverlayController {
 public:
  // Constructs the overlay widget, and adds it as a child of
  // |window_being_recorded| with the initial bounds provided.
  // |initial_bounds_in_parent| should be relative to the parent
  // |window_being_recorded|.
  RecordingOverlayController(aura::Window* window_being_recorded,
                             const gfx::Rect& initial_bounds_in_parent);
  RecordingOverlayController(const RecordingOverlayController&) = delete;
  RecordingOverlayController& operator=(const RecordingOverlayController&) =
      delete;
  ~RecordingOverlayController() = default;

  bool is_enabled() const { return is_enabled_; }

  // Toggles the overlay on or off.
  void Toggle();

  // Sets the bounds of the overlay widget. The given bounds should be relative
  // to the parent |window_being_recorded|.
  void SetBounds(const gfx::Rect& bounds_in_parent);

  // Gets the underlying window of |overlay_widget_|.
  aura::Window* GetOverlayNativeWindow();

 private:
  // Starts or stops showing the overlay on top of the recorded surface.
  void Start();
  void Stop();

  // Updates the z-order of the `overlay_widget_`'s native window.
  void UpdateWidgetStacking();

  // The overlay widget and its contents view.
  views::UniqueWidgetPtr overlay_widget_ = std::make_unique<views::Widget>();
  raw_ptr<RecordingOverlayView, ExperimentalAsh> recording_overlay_view_ =
      nullptr;

  // Whether the overlay is currently enabled and showing on top of the recorded
  // surface.
  bool is_enabled_ = false;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_RECORDING_OVERLAY_CONTROLLER_H_
