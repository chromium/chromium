// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_tray_bubble.h"
#include "ash/system/holding_space/holding_space_tray_icon.h"
#include "ash/system/tray/tray_container.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns whether the holding space model contains any finalized items.
bool ModelContainsFinalizedItems(HoldingSpaceModel* model) {
  for (const auto& item : model->items()) {
    if (item->IsFinalized())
      return true;
  }
  return false;
}

}  // namespace

// HoldingSpaceTray ------------------------------------------------------------

HoldingSpaceTray::HoldingSpaceTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  controller_observer_.Add(HoldingSpaceController::Get());
  SetVisible(false);

  // Context menu.
  if (features::IsTemporaryHoldingSpacePreviewsEnabled())
    set_context_menu_controller(this);

  // Icon.
  icon_ = tray_container()->AddChildView(
      std::make_unique<HoldingSpaceTrayIcon>(shelf));

  // It's possible that this holding space tray was created after login, such as
  // would occur if the user connects an external display. In such situations
  // the holding space model will already have been attached.
  if (HoldingSpaceController::Get()->model())
    OnHoldingSpaceModelAttached(HoldingSpaceController::Get()->model());
}

HoldingSpaceTray::~HoldingSpaceTray() = default;

void HoldingSpaceTray::ClickedOutsideBubble() {
  CloseBubble();
}

base::string16 HoldingSpaceTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE);
}

void HoldingSpaceTray::HandleLocaleChange() {
  icon_->OnLocaleChanged();
}

void HoldingSpaceTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {}

void HoldingSpaceTray::AnchorUpdated() {
  if (bubble_)
    bubble_->AnchorUpdated();
}

void HoldingSpaceTray::UpdateAfterLoginStatusChange() {
  UpdateVisibility();
}

bool HoldingSpaceTray::PerformAction(const ui::Event& event) {
  if (bubble_) {
    CloseBubble();
    return true;
  }

  ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());

  // Activate the bubble for a11y or if it was shown via keypress. Otherwise
  // focus will remain on the tray when it should enter the bubble.
  if (event.IsKeyEvent() ||
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    DCHECK(bubble_ && bubble_->GetBubbleWidget());
    bubble_->GetBubbleWidget()->widget_delegate()->SetCanActivate(true);
    bubble_->GetBubbleWidget()->Activate();
  }

  return true;
}

void HoldingSpaceTray::CloseBubble() {
  if (!bubble_)
    return;

  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kClose);

  widget_observer_.RemoveAll();

  bubble_.reset();
  SetIsActive(false);
}

void HoldingSpaceTray::ShowBubble(bool show_by_click) {
  if (bubble_)
    return;

  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kShow);

  DCHECK(tray_container());

  bubble_ = std::make_unique<HoldingSpaceTrayBubble>(this, show_by_click);

  // Observe the bubble widget so that we can do proper clean up when it is
  // being destroyed. If destruction is due to a call to `CloseBubble()` we will
  // have already cleaned up state but there are cases where the bubble widget
  // is destroyed independent of a call to `CloseBubble()`, e.g. ESC key press.
  widget_observer_.Add(bubble_->GetBubbleWidget());

  SetIsActive(true);
}

TrayBubbleView* HoldingSpaceTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

const char* HoldingSpaceTray::GetClassName() const {
  return "HoldingSpaceTray";
}

void HoldingSpaceTray::UpdateVisibility() {
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();

  const bool logged_in =
      shelf()->GetStatusAreaWidget()->login_status() == LoginStatus::USER;

  if (!model || !logged_in) {
    SetVisiblePreferred(false);
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  const bool has_ever_added_item =
      prefs ? holding_space_prefs::GetTimeOfFirstAdd(prefs).has_value() : false;
  const bool has_ever_pinned_item =
      prefs ? holding_space_prefs::GetTimeOfFirstPin(prefs).has_value() : false;

  // The holding space tray should not be visible in the shelf until the user
  // has added their first item to holding space. Once an item has been added,
  // the holding space tray will continue to be visible until the user has
  // pinned their first file. After the user has pinned their first file, the
  // holding space tray will only be visible in the shelf if their holding space
  // contains finalized items.
  SetVisiblePreferred((has_ever_added_item && !has_ever_pinned_item) ||
                      ModelContainsFinalizedItems(model));
}

base::string16 HoldingSpaceTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool HoldingSpaceTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void HoldingSpaceTray::HideBubble(const TrayBubbleView* bubble_view) {
  CloseBubble();
}

void HoldingSpaceTray::OnHoldingSpaceModelAttached(HoldingSpaceModel* model) {
  model_observer_.Add(model);
  UpdateVisibility();
}

void HoldingSpaceTray::OnHoldingSpaceModelDetached(HoldingSpaceModel* model) {
  model_observer_.Remove(model);
  UpdateVisibility();
}

void HoldingSpaceTray::OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) {
  UpdateVisibility();
}

void HoldingSpaceTray::OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) {
  UpdateVisibility();
}

void HoldingSpaceTray::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  UpdateVisibility();
}

void HoldingSpaceTray::ExecuteCommand(int command_id, int event_flags) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());
  switch (command_id) {
    case HoldingSpaceCommandId::kHidePreviews:
      holding_space_metrics::RecordPodAction(
          holding_space_metrics::PodAction::kHidePreviews);

      holding_space_prefs::SetPreviewsEnabled(
          Shell::Get()->session_controller()->GetActivePrefService(), false);
      break;
    case HoldingSpaceCommandId::kShowPreviews:
      holding_space_metrics::RecordPodAction(
          holding_space_metrics::PodAction::kShowPreviews);

      holding_space_prefs::SetPreviewsEnabled(
          Shell::Get()->session_controller()->GetActivePrefService(), true);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void HoldingSpaceTray::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  holding_space_metrics::RecordPodAction(
      holding_space_metrics::PodAction::kShowContextMenu);

  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  const bool previews_enabled = holding_space_prefs::IsPreviewsEnabled(
      Shell::Get()->session_controller()->GetActivePrefService());

  if (previews_enabled) {
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kHidePreviews,
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_HIDE_PREVIEWS),
        ui::ImageModel::FromVectorIcon(kVisibilityOffIcon));
  } else {
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kShowPreviews,
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_SHOW_PREVIEWS),
        ui::ImageModel::FromVectorIcon(kVisibilityIcon));
  }

  const int run_types = views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR;

  context_menu_runner_ =
      std::make_unique<views::MenuRunner>(context_menu_model_.get(), run_types);

  gfx::Rect anchor = source->GetBoundsInScreen();
  anchor.Inset(gfx::Insets(-kHoldingSpaceContextMenuMargin, 0));

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), /*button_controller=*/nullptr, anchor,
      views::MenuAnchorPosition::kTopLeft, source_type);
}

void HoldingSpaceTray::OnWidgetDragWillStart(views::Widget* widget) {
  // The holding space bubble should be closed while dragging holding space
  // items so as not to obstruct drop targets. Post the task to close the bubble
  // so that we don't attempt to destroy the bubble widget before the associated
  // drag event has been fully initialized.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&HoldingSpaceTray::CloseBubble,
                                weak_factory_.GetWeakPtr()));
}

void HoldingSpaceTray::OnWidgetDestroying(views::Widget* widget) {
  CloseBubble();
}

}  // namespace ash
