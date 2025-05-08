// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOCA_ON_TASK_ON_TASK_POD_VIEW_H_
#define ASH_BOCA_ON_TASK_ON_TASK_POD_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_shadow.h"
#include "ash/style/tab_slider_button.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class OnTaskPodController;
class TabSlider;

// Parameters for the OnTask pod.
inline constexpr int kPodVerticalBorder = 8;
inline constexpr int kPodHorizontalBorder = 10;
inline constexpr int kPodBorderRadius = 28;
inline constexpr int kPodVerticalPadding = 12;
inline constexpr int kPodHorizontalPadding = 12;
inline constexpr int kPodElementSpace = 8;

// Parameters for the separator in the OnTask pod.
inline constexpr int kSeparatorVerticalPadding = 6;
inline constexpr int kSeparatorHorizontalPadding = 4;
inline constexpr int kSeparatorHeight = 20;

// OnTaskPodView contains the shortcut buttons that are part of the OnTask pod.
// The OnTask pod is meant to supplement OnTask UX with convenience features
// like page navigation, tab reloads, tab strip pinning in locked mode, etc.
class ASH_EXPORT OnTaskPodView : public views::BoxLayoutView {
  METADATA_HEADER(OnTaskPodView, views::BoxLayoutView)

 public:
  explicit OnTaskPodView(OnTaskPodController* pod_controller);
  OnTaskPodView(const OnTaskPodView&) = delete;
  OnTaskPodView& operator=(const OnTaskPodView) = delete;
  ~OnTaskPodView() override;

  // views::BoxLayoutView:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Test element accessors:
  IconButton* get_back_button_for_testing() { return back_button_; }
  IconButton* get_forward_button_for_testing() { return forward_button_; }
  IconButton* reload_tab_button_for_testing() { return reload_tab_button_; }
  IconButton* pin_tab_strip_button_for_testing() {
    return pin_tab_strip_button_;
  }
  TabSliderButton* dock_left_button_for_testing() { return dock_left_button_; }
  TabSliderButton* dock_right_button_for_testing() {
    return dock_right_button_;
  }

  // Called when the web contents navigation context is updated to update
  // back_button_ and forward_button_ enable status.
  void OnPageNavigationContextUpdate();

  // Called when the boca app locked mode state is updated to update
  // pin_tab_strip_button_ enable status.
  void OnLockedModeUpdate();

 private:
  // Adds shortcut buttons to the OnTask pod view.
  void AddShortcutButtons();

  // Update the color and text of the `pin_tab_strip_button_`, and the tab strip
  // visibility.
  void UpdatePinTabStripButton(bool user_action);

  // Pointer to the pod controller that outlives the `OnTaskPodView`.
  const raw_ptr<OnTaskPodController> pod_controller_;

  // Pointer to the shadow under the pod.
  const std::unique_ptr<SystemShadow> shadow_;

  // Pointers to components hosted by the OnTask pod view.
  raw_ptr<TabSlider> pod_position_slider_;
  raw_ptr<TabSliderButton> dock_left_button_;
  raw_ptr<TabSliderButton> dock_right_button_;
  raw_ptr<views::Separator> left_separator_;
  raw_ptr<IconButton> back_button_;
  raw_ptr<IconButton> forward_button_;
  raw_ptr<IconButton> reload_tab_button_;
  raw_ptr<views::Separator> right_separator_;
  raw_ptr<IconButton> pin_tab_strip_button_;

  // Whether the tab strip should be shown or hidden.
  bool should_show_tab_strip_ = false;

  base::WeakPtrFactory<OnTaskPodView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_BOCA_ON_TASK_ON_TASK_POD_VIEW_H_
