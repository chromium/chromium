// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_BUBBLE_CONTROLLER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

enum class MouseKeysBubbleIconType;
class MouseKeysBubbleView;

// Manages the MouseKeysBubbleView.
class ASH_EXPORT MouseKeysBubbleController : public views::ViewObserver {
 public:
  MouseKeysBubbleController();
  MouseKeysBubbleController(const MouseKeysBubbleController&) = delete;
  MouseKeysBubbleController& operator=(const MouseKeysBubbleController&) =
      delete;
  ~MouseKeysBubbleController() override;

  void UpdateMouseKeysBubblePosition(gfx::Point position);

  // Updates the bubble's visibility and text content.
  void UpdateBubble(bool visible,
                    MouseKeysBubbleIconType icon,
                    const std::optional<std::u16string>& text);

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;
  base::WeakPtr<MouseKeysBubbleController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class MouseKeysBubbleControllerTest;
  friend class MouseKeysTest;

  // Performs initialization if necessary.
  void EnsureInitialize();
  // Hides widget after specified time.
  void HideWidgetAfterDelay();
  void StopTimer();
  // Updates the view and widget.
  void Update(MouseKeysBubbleIconType icon,
              const std::optional<std::u16string>& text);

  // Owned by views hierarchy.
  raw_ptr<MouseKeysBubbleView> mouse_keys_bubble_view_ = nullptr;
  raw_ptr<views::Widget> widget_ = nullptr;
  base::RetainingOneShotTimer timer_;
  base::WeakPtrFactory<MouseKeysBubbleController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_BUBBLE_CONTROLLER_H_
