// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_VIEW_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class FeaturePodButton;
class FeaturePodsContainerView;
class TopShortcutsView;
class UnifiedMediaControlsContainer;
class PageIndicatorView;
class UnifiedSystemInfoView;
class UnifiedSystemTrayController;

// Container view of slider views.
class SlidersContainerView : public views::View {
 public:
  METADATA_HEADER(SlidersContainerView);

  SlidersContainerView();

  SlidersContainerView(const SlidersContainerView&) = delete;
  SlidersContainerView& operator=(const SlidersContainerView&) = delete;

  ~SlidersContainerView() override;

  // Get height of the view.
  int GetHeight() const;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
};

// View class of the bubble in status area tray.
//
// The `QuickSettingsView` contains the quick settings controls
class ASH_EXPORT QuickSettingsView : public views::View {
 public:
  METADATA_HEADER(QuickSettingsView);

  explicit QuickSettingsView(UnifiedSystemTrayController* controller);

  QuickSettingsView(const QuickSettingsView&) = delete;
  QuickSettingsView& operator=(const QuickSettingsView&) = delete;

  ~QuickSettingsView() override;

  // Add feature pod button to `feature_pods_`.
  void AddFeaturePodButton(FeaturePodButton* button);

  // Add slider view.
  void AddSliderView(views::View* slider_view);

  // Add media controls view to `media_controls_container_`;
  void AddMediaControlsView(views::View* media_controls);

  // Hide the main view and show the given `detailed_view`.
  void SetDetailedView(views::View* detailed_view);

  // Remove the detailed view set by SetDetailedView, and show the main view.
  // It deletes `detailed_view` and children.
  void ResetDetailedView();

  // Save and restore keyboard focus of the currently focused element. Called
  // before transitioning into a detailed view.
  void SaveFocus();
  void RestoreFocus();

  // Get current height of the view (including the message center).
  int GetCurrentHeight() const;

  // Returns the number of visible feature pods.
  int GetVisibleFeaturePodCount() const;

  // Get the accessible name for the currently shown detailed view.
  std::u16string GetDetailedViewAccessibleName() const;

  // Returns true if a detailed view is being shown in the tray. (e.g Bluetooth
  // Settings).
  bool IsDetailedViewShown() const;

  // Show media controls view.
  void ShowMediaControls();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  FeaturePodsContainerView* feature_pods_container() {
    return feature_pods_container_;
  }

  View* detailed_view() { return detailed_view_container_; }
  View* detailed_view_for_testing() { return detailed_view_container_; }
  PageIndicatorView* page_indicator_view_for_test() {
    return page_indicator_view_;
  }
  UnifiedMediaControlsContainer* media_controls_container_for_testing() {
    return media_controls_container_;
  }

 private:
  class SystemTrayContainer;

  // Unowned.
  UnifiedSystemTrayController* const controller_;

  // Owned by views hierarchy.
  TopShortcutsView* top_shortcuts_view_ = nullptr;
  FeaturePodsContainerView* feature_pods_container_ = nullptr;
  PageIndicatorView* page_indicator_view_ = nullptr;
  SlidersContainerView* sliders_container_ = nullptr;
  UnifiedSystemInfoView* system_info_view_ = nullptr;
  SystemTrayContainer* system_tray_container_ = nullptr;
  views::View* detailed_view_container_ = nullptr;

  // Null if media::kGlobalMediaControlsForChromeOS is disabled.
  UnifiedMediaControlsContainer* media_controls_container_ = nullptr;

  // The view that is saved by calling SaveFocus().
  views::View* saved_focused_view_ = nullptr;

  const std::unique_ptr<ui::EventHandler> interacted_by_tap_recorder_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_VIEW_H_
