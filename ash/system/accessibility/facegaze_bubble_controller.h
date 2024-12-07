// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class FaceGazeBubbleView;

// Manages the FaceGaze bubble view.
class ASH_EXPORT FaceGazeBubbleController : public views::ViewObserver {
 public:
  FaceGazeBubbleController();
  FaceGazeBubbleController(const FaceGazeBubbleController&) = delete;
  FaceGazeBubbleController& operator=(const FaceGazeBubbleController&) = delete;
  ~FaceGazeBubbleController() override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

  // Updates the bubble's visibility and text content.
  void UpdateBubble(const std::u16string& text, bool is_warning);

 private:
  friend class AccessibilityPrivateApiTest;
  friend class FaceGazeBubbleControllerTest;

  // Performs initialization if necessary.
  void MaybeInitialize();

  // Updates the view and widget.
  void Update(const std::u16string& text, bool is_warning);

  // Called whenever the mouse enters the `FaceGazeBubbleView`.
  void OnMouseEntered();

  // Shows the `FaceGazeBubbleView`.
  void OnShowTimer();

  base::WeakPtr<FaceGazeBubbleController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Owned by views hierarchy.
  raw_ptr<FaceGazeBubbleView> facegaze_bubble_view_ = nullptr;
  raw_ptr<views::Widget> widget_ = nullptr;

  // Timer that will show the `FaceGazeBubbleView` after it elapses.
  base::RetainingOneShotTimer show_timer_;

  base::WeakPtrFactory<FaceGazeBubbleController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_CONTROLLER_H_
