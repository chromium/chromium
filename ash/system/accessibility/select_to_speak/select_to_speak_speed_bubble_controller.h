// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_SPEED_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_SPEED_BUBBLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_speed_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/memory/raw_ptr.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

// Manages the Select-to-speak reading speed (speech rate) selector.
class ASH_EXPORT SelectToSpeakSpeedBubbleController
    : public TrayBubbleView::Delegate,
      public SelectToSpeakSpeedView::Delegate,
      public ::wm::ActivationChangeObserver {
 public:
  explicit SelectToSpeakSpeedBubbleController(
      SelectToSpeakSpeedView::Delegate* delegate);
  SelectToSpeakSpeedBubbleController(
      const SelectToSpeakSpeedBubbleController&) = delete;
  SelectToSpeakSpeedBubbleController& operator=(
      const SelectToSpeakSpeedBubbleController&) = delete;
  ~SelectToSpeakSpeedBubbleController() override;

  // Displays the reading speed options bubble anchored to the given rect.
  void Show(views::View* anchor_view, double speech_rate);

  // Hides the reading speed options bubble.
  void Hide();

  // Whether the bubble is visible.
  bool IsVisible() const;

 private:
  friend class SelectToSpeakSpeedBubbleControllerTest;

  void MaybeRecordDurationHistogram();

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  void BubbleViewDestroyed() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // SelectToSpeakSpeedView::Delegate:
  void OnSpeechRateSelected(double speech_rate) override;

  base::Time last_show_time_;

  // Owned by views hierarchy.
  raw_ptr<TrayBubbleView> bubble_view_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  raw_ptr<SelectToSpeakSpeedView, DanglingUntriaged> speed_view_ = nullptr;

  // Owned by parent whose lifetime exceeds this class.
  raw_ptr<SelectToSpeakSpeedView::Delegate> delegate_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_SPEED_BUBBLE_CONTROLLER_H_
