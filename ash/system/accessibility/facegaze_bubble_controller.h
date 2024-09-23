// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
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
  void UpdateBubble(const std::u16string& text);

 private:
  friend class AccessibilityPrivateApiTest;
  friend class FaceGazeBubbleControllerTest;

  // Performs initialization if necessary.
  void MaybeInitialize();

  // Updates the view and widget.
  void Update(const std::u16string& text);

  // Owned by views hierarchy.
  raw_ptr<FaceGazeBubbleView> facegaze_bubble_view_ = nullptr;
  raw_ptr<views::Widget> widget_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_CONTROLLER_H_
