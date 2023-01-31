// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_VIEW_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "ash/system/brightness/unified_brightness_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class FlexLayoutView;
}  // namespace views

namespace ash {

class FeatureTile;
class FeatureTilesContainerView;
class PageIndicatorView;
class QuickSettingsFooter;
class QuickSettingsHeader;
class UnifiedMediaControlsContainer;
class UnifiedSystemTrayController;

// View class of the bubble in status area tray.
//
// The `QuickSettingsView` contains the quick settings controls
class ASH_EXPORT QuickSettingsView : public views::View,
                                     public PaginationModelObserver {
 public:
  METADATA_HEADER(QuickSettingsView);

  explicit QuickSettingsView(UnifiedSystemTrayController* controller);

  QuickSettingsView(const QuickSettingsView&) = delete;
  QuickSettingsView& operator=(const QuickSettingsView&) = delete;

  ~QuickSettingsView() override;

  // Sets the maximum height that the view can take.
  void SetMaxHeight(int max_height);

  // Adds tiles to the FeatureTile container view.
  void AddTiles(std::vector<std::unique_ptr<FeatureTile>> tiles);

  // Adds slider view.
  void AddSliderView(views::View* slider_view);

  // Adds media controls view to `media_controls_container_`;
  void AddMediaControlsView(views::View* media_controls);

  // Hides the main view and shows the given `detailed_view`.
  void SetDetailedView(std::unique_ptr<views::View> detailed_view);

  // Removes the detailed view set by SetDetailedView, and shows the main view.
  // It deletes `detailed_view` and children.
  void ResetDetailedView();

  // Saves and restores keyboard focus of the currently focused element. Called
  // before transitioning into a detailed view.
  void SaveFocus();
  void RestoreFocus();

  // Gets current height of the view (including the message center).
  int GetCurrentHeight() const;

  // Calculates how many rows to use based on the max available height.
  // FeatureTilesContainer can adjust it's height by reducing the number of rows
  // it uses.
  int CalculateHeightForFeatureTilesContainer();

  // Gets the accessible name for the currently shown detailed view.
  std::u16string GetDetailedViewAccessibleName() const;

  // Returns true if a detailed view is being shown in the tray. (e.g Bluetooth
  // Settings).
  bool IsDetailedViewShown() const;

  // Shows media controls view.
  void ShowMediaControls();

  // PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override;

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override;

  FeatureTilesContainerView* feature_tiles_container() {
    return feature_tiles_container_;
  }

  views::View* detailed_view() { return detailed_view_container_; }
  views::View* detailed_view_for_testing() { return detailed_view_container_; }
  PageIndicatorView* page_indicator_view_for_test() {
    return page_indicator_view_;
  }
  UnifiedMediaControlsContainer* media_controls_container_for_testing() {
    return media_controls_container_;
  }

 private:
  class SystemTrayContainer;
  friend class UnifiedBrightnessViewTest;
  friend class UnifiedVolumeViewTest;

  // Owned by UnifiedSystemTrayBubble.
  UnifiedSystemTrayController* const controller_;

  // Owned by views hierarchy.
  views::FlexLayoutView* system_tray_container_ = nullptr;
  QuickSettingsHeader* header_ = nullptr;
  FeatureTilesContainerView* feature_tiles_container_ = nullptr;
  PageIndicatorView* page_indicator_view_ = nullptr;
  views::FlexLayoutView* sliders_container_ = nullptr;
  QuickSettingsFooter* footer_ = nullptr;
  views::View* detailed_view_container_ = nullptr;

  // Null if media::kGlobalMediaControlsForChromeOS is disabled.
  UnifiedMediaControlsContainer* media_controls_container_ = nullptr;

  // The maximum height available to the view.
  int max_height_ = 0;

  // The view that is saved by calling SaveFocus().
  views::View* saved_focused_view_ = nullptr;

  const std::unique_ptr<ui::EventHandler> interacted_by_tap_recorder_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_VIEW_H_
