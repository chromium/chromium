// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_BUBBLE_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_BUBBLE_H_

#include <memory>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/tray/time_to_click_recorder.h"
#include "ash/system/tray/tray_bubble_base.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ui {
class LayerOwner;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

class UnifiedSystemTray;
class UnifiedSystemTrayController;
class UnifiedSystemTrayView;

// Manages the bubble that contains UnifiedSystemTrayView.
// Shows the bubble on the constructor, and closes the bubble on the destructor.
// It is possible that the bubble widget is closed on deactivation. In such
// case, this class calls UnifiedSystemTray::CloseBubble() to delete itself.
class ASH_EXPORT UnifiedSystemTrayBubble
    : public TrayBubbleBase,
      public ash::ScreenLayoutObserver,
      public views::WidgetObserver,
      public ShelfObserver,
      public ::wm::ActivationChangeObserver,
      public TimeToClickRecorder::Delegate,
      public TabletModeObserver {
 public:

  explicit UnifiedSystemTrayBubble(UnifiedSystemTray* tray, bool show_by_click);
  ~UnifiedSystemTrayBubble() override;

  // Return the bounds of the bubble in the screen.
  gfx::Rect GetBoundsInScreen() const;

  // True if the bubble is active.
  bool IsBubbleActive() const;

  // Activate the system tray bubble.
  void ActivateBubble();

  // Close the bubble immediately.
  void CloseNow();

  // Collapse the message center bubble.
  void CollapseMessageCenter();

  // Expand the message center bubble.
  void ExpandMessageCenter();

  // Ensure the bubble is collapsed.
  void EnsureCollapsed();

  // Ensure the bubble is expanded.
  void EnsureExpanded();

  // Show audio settings detailed view.
  void ShowAudioDetailedView();

  // Show network settings detailed view.
  void ShowNetworkDetailedView(bool force);

  // Update bubble bounds and focus if necessary.
  void UpdateBubble();

  // Update layer transform during expand / collapse animation. During
  // animation, the height of the view changes, but resizing of the bubble is
  // performance bottleneck. This method makes use of layer transform to avoid
  // resizing of the bubble during animation.
  void UpdateTransform();

  // Return the maximum height available for both the system tray and
  // the message center.
  int CalculateMaxHeight() const;

  // Return the current visible height of the tray, even when partially
  // collapsed / expanded.
  int GetCurrentTrayHeight() const;

  // Relinquish focus and transfer it to the message center widget.
  bool FocusOut(bool reverse);

  // Inform UnifiedSystemTrayView of focus being acquired.
  void FocusEntered(bool reverse);

  // TrayBubbleBase:
  TrayBackgroundView* GetTray() const override;
  TrayBubbleView* GetBubbleView() const override;
  views::Widget* GetBubbleWidget() const override;

  // ash::ScreenLayoutObserver:
  void OnDisplayConfigurationChanged() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // TimeToClickRecorder::Delegate:
  void RecordTimeToClick() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ShelfObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  UnifiedSystemTrayView* unified_view() { return unified_view_; }

  UnifiedSystemTrayController* controller_for_test() {
    return controller_.get();
  }

 private:
  friend class UnifiedSystemTrayTestApi;

  void UpdateBubbleBounds();

  // Create / destroy background blur layer that is used during animation.
  void CreateBlurLayerForAnimation();
  void DestroyBlurLayerForAnimation();

  // Set visibility of bubble frame border. Used for disabling the border during
  // animation.
  void SetFrameVisible(bool visible);

  // Returns the insets for the bubble.
  gfx::Insets GetInsets();

  // Controller of UnifiedSystemTrayView. As the view is owned by views
  // hierarchy, we have to own the controller here.
  std::unique_ptr<UnifiedSystemTrayController> controller_;

  // Owner of this class.
  UnifiedSystemTray* tray_;

  // Widget that contains UnifiedSystemTrayView. Unowned.
  // When the widget is closed by deactivation, |bubble_widget_| pointer is
  // invalidated and we have to delete UnifiedSystemTrayBubble by calling
  // UnifiedSystemTray::CloseBubble().
  // In order to do this, we observe OnWidgetDestroying().
  views::Widget* bubble_widget_ = nullptr;

  // PreTargetHandler of |unified_view_| to record TimeToClick metrics. Owned.
  std::unique_ptr<TimeToClickRecorder> time_to_click_recorder_;

  // The time the bubble is created. If the bubble is not created by button
  // click (|show_by_click| in ctor is false), it is not set.
  base::Optional<base::TimeTicks> time_shown_by_click_;

  // Background blur layer that is used during animation.
  std::unique_ptr<ui::LayerOwner> blur_layer_;

  TrayBubbleView* bubble_view_ = nullptr;
  UnifiedSystemTrayView* unified_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemTrayBubble);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_TRAY_BUBBLE_H_
