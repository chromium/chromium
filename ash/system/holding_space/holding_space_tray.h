// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/drag_drop/scoped_drag_drop_observer.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_tray_bubble.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/layer_tree_owner.h"
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
class ProgressIndicator;

// The HoldingSpaceTray shows the tray button in the bottom area of the screen.
// This class also controls the lifetime for all of the tools available in the
// palette. HoldingSpaceTray has one instance per-display.
class ASH_EXPORT HoldingSpaceTray : public TrayBackgroundView,
                                    public HoldingSpaceControllerObserver,
                                    public HoldingSpaceModelObserver,
                                    public SessionObserver,
                                    public ui::SimpleMenuModel::Delegate,
                                    public views::WidgetObserver {
  METADATA_HEADER(HoldingSpaceTray, TrayBackgroundView)

 public:
  explicit HoldingSpaceTray(Shelf* shelf);
  HoldingSpaceTray(const HoldingSpaceTray& other) = delete;
  HoldingSpaceTray& operator=(const HoldingSpaceTray& other) = delete;
  ~HoldingSpaceTray() override;

  // TrayBackgroundView:
  void Initialize() override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  std::u16string GetAccessibleNameForTray() override;
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& point) const override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void UpdateAfterLoginStatusChange() override;
  void CloseBubbleInternal() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  void SetVisiblePreferred(bool visible_preferred) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;
  void Layout(PassKey) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void OnThemeChanged() override;
  void OnShouldShowAnimationChanged(bool should_animate) override;
  std::unique_ptr<ui::SimpleMenuModel> CreateContextMenuModel() override;
  void UpdateTrayItemColor(bool is_active) override;

  // Invoke to cause the holding space tray to recalculate and update its
  // visibility. Note that this may or may not result in a visibility change
  // depending on state.
  void UpdateVisibility();

  // Returns the holding space tray bubble for testing.
  HoldingSpaceTrayBubble* bubble_for_testing() { return bubble_.get(); }

  // Previews are updated with delay to de-dupe against multiple updates
  // scheduled in quick succession. Invoke this method to cause scheduled
  // updates to be run immediately for testing.
  void FirePreviewsUpdateTimerIfRunningForTesting();

  // Previews are updated with delay to de-dupe against multiple updates
  // scheduled in quick success. This method allows updates to be scheduled with
  // zero delay, causing them to instead run immediately, for testing.
  void set_use_zero_previews_update_delay_for_testing(bool zero_delay) {
    use_zero_previews_update_delay_ = zero_delay;
  }

 private:
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
  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // views::WidgetObserver:
  void OnWidgetDragWillStart(views::Widget* widget) override;

  // Registers pref change registrars for preferences relevant to the holding
  // space tray state.
  void ObservePrefService(PrefService* prefs);

  // Callback called when this TrayBackgroundView is pressed.
  void OnTrayButtonPressed(const ui::Event& event);

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

  // Updates the `default_tray_icon_` to account for potential overlap with the
  // inner icon of the `progress_indicator_` as both may occupy the same space.
  void UpdateDefaultTrayIcon();

  // Updates this view (and its children) to reflect state as a potential drop
  // target. If `event` is `nullptr`, this view is *not* a drop target.
  // Otherwise this view is a drop target if the `event` is located within
  // sufficient range of its bounds and contains pinnable files.
  void UpdateDropTargetState(ScopedDragDropObserver::EventType event_type,
                             const ui::DropTargetEvent* event);

  // Sets whether tray visibility and previews updates should be animated.
  void SetShouldAnimate(bool should_animate);

  // Handles the specified drop `event` by pinning associated files to the tray.
  void PerformDrop(const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  std::unique_ptr<HoldingSpaceTrayBubble> bubble_;
  std::unique_ptr<aura::client::DragDropClientObserver> drag_drop_observer_;

  // Default tray icon shown when there are no previews available (or the
  // previews are disabled).
  // Owned by views hierarchy.
  raw_ptr<views::ImageView> default_tray_icon_ = nullptr;

  // Content forward tray icon that contains holding space item previews.
  // Owned by views hierarchy.
  raw_ptr<HoldingSpaceTrayIcon> previews_tray_icon_ = nullptr;

  // The view drawn on top of all other child views to indicate that this
  // view is a drop target capable of handling the current drag payload.
  raw_ptr<views::View> drop_target_overlay_ = nullptr;

  // The icon parented by the `drop_target_overlay_` to indicate that this view
  // is a drop target capable of handling the current drag payload.
  raw_ptr<views::ImageView> drop_target_icon_ = nullptr;

  // Owns the `ui::Layer` which paints indication of progress for all holding
  // space items in the model attached to the holding space controller.
  // NOTE: The `ui::Layer` is *not* painted if there are no items in progress.
  std::unique_ptr<ProgressIndicator> progress_indicator_;

  // Subscription to receive notification of changes to the
  // `progress_indicator_`'s underlying progress.
  base::RepeatingClosureList::Subscription
      progress_indicator_progress_changed_callback_list_subscription_;

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

  // Whether the user is currently dragging data which can be dropped on the
  // tray as part of a drag-and-drop to pin action. Note that this value is only
  // present while a drag is in progress and the holding space tray is visible.
  std::optional<bool> can_drop_to_pin_;

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

  // Animation will be disabled for the lifetime of this variable.
  std::unique_ptr<base::ScopedClosureRunner> animation_disabler_;

  base::WeakPtrFactory<HoldingSpaceTray> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_
