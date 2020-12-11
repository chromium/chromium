// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_H_

#include <memory>

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
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class PrefChangeRegistrar;

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
  explicit HoldingSpaceTray(Shelf* shelf);
  HoldingSpaceTray(const HoldingSpaceTray& other) = delete;
  HoldingSpaceTray& operator=(const HoldingSpaceTray& other) = delete;
  ~HoldingSpaceTray() override;

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void UpdateAfterLoginStatusChange() override;
  bool PerformAction(const ui::Event& event) override;
  void CloseBubble() override;
  void ShowBubble(bool show_by_click) override;
  TrayBubbleView* GetBubbleView() override;
  const char* GetClassName() const override;

 private:
  void UpdateVisibility();

  // Updates the visibility of the tray icon showing item previews.
  // If the previews are not enabled, or the holding space is empty, the default
  // holding space tray icon will be shown.
  void UpdatePreviewsVisibility();

  // TrayBubbleView::Delegate:
  base::string16 GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // HoldingSpaceControllerObserver:
  void OnHoldingSpaceModelAttached(HoldingSpaceModel* model) override;
  void OnHoldingSpaceModelDetached(HoldingSpaceModel* model) override;

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

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

  // Whether previews icon is currently shown. Note that if the previews
  // feature is disabled, this will always be false. Otherwise, previews can be
  // enabled/ disabled by the user at runtime.
  bool PreviewsShown() const;

  std::unique_ptr<HoldingSpaceTrayBubble> bubble_;
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  // Default tray icon shown when there are no previews available (or the
  // previews are disabled).
  // Owned by views hierarchy.
  views::ImageView* default_tray_icon_ = nullptr;

  // Content forward tray icon that contains holding space item previews.
  // Owned by views hierarchy.
  HoldingSpaceTrayIcon* previews_tray_icon_ = nullptr;

  // When the holding space previews feature is enabled, the user can enable/
  // disable previews at runtime. This registrar is associated with the active
  // user pref service and notifies the holding space tray icon of changes to
  // the user's preference.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

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
