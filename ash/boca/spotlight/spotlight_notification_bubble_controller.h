// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_BUBBLE_CONTROLLER_H_
#define ASH_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_BUBBLE_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

namespace ash {

// X() Location of the notification bubble widget.
enum class WidgetLocation { kLeft, kRight };

// Controller for the Spotlight notification bubble. Owns the view widget
// and provides the active teacher to the bubble's label.
// TODO: dorianbrandon - Currently the notification bubble can look awkwardly
// placed during window change events when switching between locked mode or
// unlocked mode. This can be fixed by the user manually mousing over the
// widget which will cause an update, however we should solve this
// automatically.
class ASH_EXPORT SpotlightNotificationBubbleController
    : public ui::EventObserver {
 public:
  SpotlightNotificationBubbleController();
  SpotlightNotificationBubbleController(
      const SpotlightNotificationBubbleController&) = delete;
  SpotlightNotificationBubbleController& operator=(
      const SpotlightNotificationBubbleController&) = delete;
  ~SpotlightNotificationBubbleController() override;

  views::Widget* GetNotificationWidgetForTesting() {
    return notification_widget_.get();
  }
  WidgetLocation GetWidgetLocationForTesting() { return location_; }

  // ui::EventObserver
  void OnEvent(const ui::Event& event) override;

  // Shows the notification bubble widget with the current teacher.
  void ShowNotificationBubble(const std::string& teacher_name);

  // Hides the notification bubble widget.
  void HideNotificationBubble();

  // Returns whether the notification bubble is visible on screen.
  bool IsNotificationBubbleVisible();

  void OnSessionEnded();

 private:
  const gfx::Rect CalculateWidgetBounds();

  void CloseNotificationBubbleNow();

  WidgetLocation location_ = WidgetLocation::kRight;
  std::unique_ptr<views::Widget> notification_widget_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};
}  // namespace ash

#endif  // ASH_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_BUBBLE_CONTROLLER_H_
