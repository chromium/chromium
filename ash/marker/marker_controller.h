// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MARKER_MARKER_CONTROLLER_H_
#define ASH_MARKER_MARKER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/fast_ink/fast_ink_pointer_controller.h"
#include "ash/fast_ink/fast_ink_points.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class HighlighterView;

// A checked observer which receives notification of changes to the marker
// activation state.
class ASH_EXPORT MarkerObserver : public base::CheckedObserver {
 public:
  virtual void OnMarkerStateChanged(bool enabled) {}
};

// Controller for the Marker functionality. Enables/disables Marker as well as
// receives points and passes them off to be rendered.
class ASH_EXPORT MarkerController : public fast_ink::FastInkPointerController {
 public:
  MarkerController();
  MarkerController(const MarkerController&) = delete;
  MarkerController& operator=(const MarkerController&) = delete;
  ~MarkerController() override;

  static MarkerController* Get();

  // Adds/removes the specified `observer`.
  void AddObserver(MarkerObserver* observer);
  void RemoveObserver(MarkerObserver* observer);

  // Clears marker pointer.
  void Clear();
  // Clears the last stroke.
  void UndoLastStroke();

  void ChangeColor(SkColor new_color);

  // fast_ink::FastInkPointerController:
  void SetEnabled(bool enabled) override;

 private:
  friend class MarkerControllerTestApi;

  // Destroys `marker_view_widget_`, if it exists.
  void DestroyMarkerView();
  // Returns the marker view in use, or nullptr.
  // TODO(llin): Consider renaming HighlighterView to DrawingView.
  HighlighterView* GetMarkerView();
  // Notifies observers when state changed.
  void NotifyStateChanged(bool enabled);

  // fast_ink::FastInkPointerController:
  views::View* GetPointerView() const override;
  void CreatePointerView(base::TimeDelta presentation_delay,
                         aura::Window* root_window) override;
  void UpdatePointerView(ui::TouchEvent* event) override;
  void UpdatePointerView(ui::MouseEvent* event) override;
  void DestroyPointerView() override;
  bool CanStartNewGesture(ui::LocatedEvent* event) override;
  bool ShouldProcessEvent(ui::LocatedEvent* event) override;

  // `marker_view_widget_` will only hold an instance when the Marker is enabled
  // and activated (pressed or dragged) and until cleared.
  views::UniqueWidgetPtr marker_view_widget_;
  HighlighterView* marker_view_ = nullptr;
  SkColor marker_color_ = fast_ink::FastInkPoints::kDefaultColor;

  base::ObserverList<MarkerObserver> observers_;

  base::WeakPtrFactory<MarkerController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_MARKER_MARKER_CONTROLLER_H_
