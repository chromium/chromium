// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_tray_bubble.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class PrefChangeRegistrar;

namespace aura {
namespace client {
class DragDropClientObserver;
}  // namespace client
}  // namespace aura

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class HoldingSpaceTrayIcon;

// The HoldingSpaceTray shows the tray button in the bottom area of the screen.
// This class also controls the lifetime for all of the tools available in the
// palette. HoldingSpaceTray has one instance per-display.
class ASH_EXPORT HoldingSpaceTray : public TrayBackgroundView,
                                    public HoldingSpaceControllerObserver,
                                    public HoldingSpaceModelObserver,
                                    public SessionObserver,
                                    public ui::SimpleMenuModel::Delegate,
                                    public views::ContextMenuController,
                                    public views::WidgetObserver {
 public:
  METADATA_HEADER(HoldingSpaceTray);

  explicit HoldingSpaceTray(Shelf* shelf);
  HoldingSpaceTray(const HoldingSpaceTray& other) = delete;
  HoldingSpaceTray& operator=(const HoldingSpaceTray& other) = delete;
  ~HoldingSpaceTray() override;

  // TrayBackgroundView:
  void Initialize() override;
  void ClickedOutsideBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& point) const override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void UpdateAfterLoginStatusChange() override;
  void CloseBubble() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void SetVisiblePreferred(bool visible_preferred) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  ui::mojom::DragOperation OnPerformDrop(
      const ui::DropTargetEvent& event) override;
  void Layout() override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  void set_use_zero_previews_update_delay_for_testing(bool zero_delay) {
    use_zero_previews_update_delay_ = zero_delay;
  }

  void FirePreviewsUpdateTimerIfRunningForTesting();

 private:
  void UpdateVisibility();

  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override;
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::WidgetObserver:
  void OnWidgetDragWillStart(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Registers pref change registrars for preferences relevant to the holding
  // space tray state.
  void ObservePrefService(PrefService* prefs);

  // Called when the state reflected in the previews icon changes - it updates
  // the previews icon visibility and schedules the previews icon update.
  void UpdatePreviewsState();

  // Updates the visibility of the tray icon showing item previews.
  // If the previews are not enabled, or the holding space is empty, the default
  // holding space tray icon will be shown.
  void UpdatePreviewsVisibility();

  // Schedules a task to update the list of items shown in the previews tray
  // icon.
  void SchedulePreviewsIconUpdate();

  // Calculates the set of items that should be added to the holding space
  // preview icon, and updates the icon state. No-op if previews are not
  // enabled.
  void UpdatePreviewsIcon();

  // Whether previews icon is currently shown. Note that if the previews
  // feature is disabled, this will always be false. Otherwise, previews can be
  // enabled/ disabled by the user at runtime.
  bool PreviewsShown() const;

  // Updates this view (and its children) to reflect state as a potential drop
  // target. If `event` is `nullptr`, this view is *not* a drop target.
  // Otherwise this view is a drop target iff the `event` is located within
  // sufficient range of its bounds and contains pinnable files.
  void UpdateDropTargetState(const ui::DropTargetEvent* event);

  // Sets whether tray visibility and previews updates should be animated.
  void SetShouldAnimate(bool should_animate);

  std::unique_ptr<HoldingSpaceTrayBubble> bubble_;
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<aura::client::DragDropClientObserver> drag_drop_observer_;

  // Default tray icon shown when there are no previews available (or the
  // previews are disabled).
  // Owned by views hierarchy.
  views::ImageView* default_tray_icon_ = nullptr;

  // Content forward tray icon that contains holding space item previews.
  // Owned by views hierarchy.
  HoldingSpaceTrayIcon* previews_tray_icon_ = nullptr;

  // The view drawn on top of all other child views to indicate that this
  // view is a drop target capable of handling the current drag payload.
  views::View* drop_target_overlay_ = nullptr;

  // When the holding space previews feature is enabled, the user can enable/
  // disable previews at runtime. This registrar is associated with the active
  // user pref service and notifies the holding space tray icon of changes to
  // the user's preference.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Timer for updating previews shown in the content forward tray icon.
  base::OneShotTimer previews_update_;

  // Used in tests to shorten the timeout for updating previews in the content
  // forward tray icon.
  bool use_zero_previews_update_delay_ = false;

  // Whether the user performed a drag-and-drop to pin action. Note that this
  // flag is set only within the scope of a drop release event sequence. It is
  // otherwise always set to `false`.
  bool did_drop_to_pin_ = false;

  base::ScopedObservation<HoldingSpaceController,
                          HoldingSpaceControllerObserver>
      controller_observer_{this};
  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observer_{this};
  base::ScopedObservation<SessionController, SessionObserver> session_observer_{
      this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observer_{this};

  base::WeakPtrFactory<HoldingSpaceTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_
