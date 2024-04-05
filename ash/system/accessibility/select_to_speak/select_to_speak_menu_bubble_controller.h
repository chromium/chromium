// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_MENU_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_MENU_BUBBLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_menu_view.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_speed_bubble_controller.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

// Manages the Select-to-speak floating menu panel.
class ASH_EXPORT SelectToSpeakMenuBubbleController
    : public TrayBubbleView::Delegate,
      public SelectToSpeakMenuView::Delegate,
      public SelectToSpeakSpeedView::Delegate,
      public ::wm::ActivationChangeObserver {
 public:
  SelectToSpeakMenuBubbleController();
  ~SelectToSpeakMenuBubbleController() override;

  // Displays the floating menu panel anchored to the given rect.
  void Show(const gfx::Rect& anchor,
            bool is_paused,
            double initial_speech_rate);

  // Hides the floating menu panel.
  void Hide();

 private:
  friend class SelectToSpeakMenuBubbleControllerTest;
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

  // SelectToSpeakMenuView::Delegate:
  void OnActionSelected(SelectToSpeakPanelAction action) override;

  // SelectToSpeakSpeedView::Delegate:
  void OnSpeechRateSelected(double speech_rate) override;

  // Owned by views hierarchy.
  raw_ptr<TrayBubbleView> bubble_view_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  raw_ptr<SelectToSpeakMenuView> menu_view_ = nullptr;

  std::unique_ptr<SelectToSpeakSpeedBubbleController> speed_bubble_controller_;
  double initial_speech_rate_ = 1.0;
  base::Time last_show_time_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_MENU_BUBBLE_CONTROLLER_H_
