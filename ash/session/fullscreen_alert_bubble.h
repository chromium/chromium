// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FULLSCREEN_ALERT_BUBBLE_H_
#define ASH_FULLSCREEN_ALERT_BUBBLE_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"

namespace views {
class Button;
class Widget;
}  // namespace views

namespace gfx {
class Rect;
}

namespace base {
class OneShotTimer;
}

namespace ash {

class FullscreenAlertBubbleView;

// When the device returns back from sleep or low brightness without a lock
// screen, remind the user to exit fullscreen before entering password.
class ASH_EXPORT FullscreenAlertBubble {
 public:
  FullscreenAlertBubble();
  ~FullscreenAlertBubble();

  FullscreenAlertBubble(const FullscreenAlertBubble&) = delete;
  FullscreenAlertBubble& operator=(const FullscreenAlertBubble&) = delete;

  void Show();
  void Hide();

  void Dismiss(const ui::Event& event);
  void ExitFullscreen(const ui::Event& event);

 private:
  gfx::Rect CalculateBubbleBounds();

  std::unique_ptr<FullscreenAlertBubbleView> bubble_view_;

  std::unique_ptr<views::Widget> bubble_widget_;

  std::unique_ptr<base::OneShotTimer> timer_;

  base::WeakPtrFactory<FullscreenAlertBubble> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_FULLSCREEN_ALERT_BUBBLE_H_
