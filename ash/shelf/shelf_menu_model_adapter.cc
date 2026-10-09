// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_menu_model_adapter.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_bubble.h"
#include "ash/shelf/shelf_tooltip_bubble.h"
#include "ash/shelf/shelf_window_preview_bubble.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

ShelfMenuModelAdapter::ShelfMenuModelAdapter(
    const std::string& app_id,
    std::unique_ptr<ui::SimpleMenuModel> model,
    views::View* menu_owner,
    ui::mojom::MenuSourceType source_type,
    base::OnceClosure on_menu_closed_callback,
    bool is_tablet_mode,
    bool for_application_menu_items,
    base::WeakPtr<ShelfItemDelegate> item_delegate)
    : AppMenuModelAdapter(app_id,
                          std::move(model),
                          menu_owner->GetWidget(),
                          source_type,
                          std::move(on_menu_closed_callback),
                          is_tablet_mode),
      for_application_menu_items_(for_application_menu_items),
      item_delegate_(std::move(item_delegate)),
      menu_owner_(menu_owner) {
  menu_owner_observation_.Observe(menu_owner_);
}

ShelfMenuModelAdapter::~ShelfMenuModelAdapter() {
  CloseHoverBubble(/*animate=*/false);
}

views::MenuItemView* ShelfMenuModelAdapter::AppendMenuItem(
    views::MenuItemView* menu,
    ui::MenuModel* model,
    size_t index) {
  views::MenuItemView* item =
      AppMenuModelAdapter::AppendMenuItem(menu, model, index);
  if (features::IsWindowPreviewOnShelfEnabled() &&
      for_application_menu_items_ && item) {
    selection_subscriptions_.push_back(item->AddSelectedChangedCallback(
        base::BindRepeating(&ShelfMenuModelAdapter::OnMenuItemSelectedChanged,
                            base::Unretained(this), item)));
  }
  return item;
}

void ShelfMenuModelAdapter::OnMenuClosed(views::MenuItemView* menu) {
  CloseHoverBubble(/*animate=*/false);
  selection_subscriptions_.clear();
  menu_owner_observation_.Reset();
  menu_owner_ = nullptr;
  AppMenuModelAdapter::OnMenuClosed(menu);
}

void ShelfMenuModelAdapter::OnViewIsDeleting(views::View* observed_view) {
  CHECK_EQ(menu_owner_, observed_view);
  menu_owner_observation_.Reset();
  menu_owner_ = nullptr;
}

void ShelfMenuModelAdapter::OnMenuItemSelectedChanged(
    views::MenuItemView* item) {
  CHECK(item);

  if (!item->IsSelected()) {
    if (hover_anchor_view_ == item) {
      hover_anchor_view_ = nullptr;
      if (preview_bubble_ || tooltip_bubble_) {
        // Delay closing slightly so that if another menu item is selected
        // immediately after, the existing bubble can be reused or replaced
        // without a gap.
        hover_timer_.Start(
            FROM_HERE, kClosePreviewDelay,
            base::BindOnce(&ShelfMenuModelAdapter::CloseHoverBubble,
                           base::Unretained(this), /*animate=*/true));
      } else {
        hover_timer_.Stop();
      }
    }
    return;
  }

  hover_timer_.Stop();

  base::expected<aura::Window*, std::u16string> result =
      item_delegate_ ? item_delegate_->GetAppMenuItemWindow(item->GetCommand())
                     : base::unexpected(std::u16string());
  if (result.has_value() && preview_bubble_) {
    // When switching between items with an active preview bubble, update the
    // existing bubble in place to avoid fade-out-in animation.
    hover_anchor_view_ = item;
    preview_bubble_->UpdateAnchorAndWindow(item, result.value());
    return;
  }

  const bool was_showing = preview_bubble_ || tooltip_bubble_;
  CloseHoverBubble(/*animate=*/true);
  if (!result.has_value() && result.error().empty()) {
    return;
  }

  hover_anchor_view_ = item;
  if (was_showing) {
    // When switching from another item with an active bubble, replace it
    // immediately.
    ShowHoverBubble();
  } else {
    hover_timer_.Start(FROM_HERE, kShowPreviewDelay,
                       base::BindOnce(&ShelfMenuModelAdapter::ShowHoverBubble,
                                      base::Unretained(this)));
  }
}

void ShelfMenuModelAdapter::ShowHoverBubble() {
  CHECK(!preview_bubble_);
  CHECK(!tooltip_bubble_);
  CHECK(hover_anchor_view_);
  views::MenuItemView* anchor_view = hover_anchor_view_;
  hover_anchor_view_ = nullptr;
  if (!IsShowingMenu() || !item_delegate_) {
    return;
  }
  base::expected<aura::Window*, std::u16string> result =
      item_delegate_->GetAppMenuItemWindow(anchor_view->GetCommand());
  if (!result.has_value() && result.error().empty()) {
    return;
  }

  hover_anchor_view_ = anchor_view;
  ShelfBubble* bubble = nullptr;
  if (result.has_value()) {
    preview_bubble_ = new ShelfWindowPreviewBubble(anchor_view, result.value());
    bubble = preview_bubble_;
  } else {
    // Anchor the tooltip beside the menu, on the same side as the preview
    // bubble, so the two line up as the user moves between items.
    tooltip_bubble_ = new ShelfTooltipBubble(
        anchor_view, ShelfAlignment::kBottom, result.error(),
        ShelfBubble::GetArrowForMenuItem(anchor_view));
    bubble = tooltip_bubble_;
    // Show the tooltip without animation, as it is shown and replaced while
    // the user moves between menu items.
    ::wm::SetWindowVisibilityAnimationTransition(
        bubble->GetWidget()->GetNativeWindow(), ::wm::ANIMATE_NONE);
    bubble->GetWidget()->Show();
  }
  bubble->RegisterWindowClosingCallback(base::BindOnce(
      &ShelfMenuModelAdapter::OnHoverBubbleClosing, base::Unretained(this)));
}

void ShelfMenuModelAdapter::CloseHoverBubble(bool animate) {
  hover_timer_.Stop();
  hover_anchor_view_ = nullptr;
  if (preview_bubble_) {
    if (animate) {
      preview_bubble_.ExtractAsDangling()->FadeOutAndClose();
    } else {
      preview_bubble_.ExtractAsDangling()->GetWidget()->CloseNow();
    }
  }
  if (tooltip_bubble_) {
    tooltip_bubble_.ExtractAsDangling()->GetWidget()->CloseNow();
  }
}

void ShelfMenuModelAdapter::OnHoverBubbleClosing() {
  preview_bubble_ = nullptr;
  tooltip_bubble_ = nullptr;
  hover_anchor_view_ = nullptr;
  hover_timer_.Stop();
}

int ShelfMenuModelAdapter::GetCommandIdForHistograms(int command_id) {
  if (!for_application_menu_items_)
    return command_id;

  // Command IDs of shelf app menu items must be in the range
  // [APP_MENU_ITEM_ID_FIRST : APP_MENU_ITEM_ID_LAST] to avoid conflicts with
  // other IDs when reporting UMA.
  command_id += APP_MENU_ITEM_ID_FIRST;
  DCHECK_LE(command_id, APP_MENU_ITEM_ID_LAST);
  return command_id;
}

void ShelfMenuModelAdapter::RecordHistogramOnMenuClosed() {
  base::TimeDelta user_journey_time = base::TimeTicks::Now() - menu_open_time();
  // If the menu is for a ShelfButton.
  if (!app_id().empty()) {
    UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTimeV2.ShelfButton",
                        user_journey_time);
    UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSourceV2.ShelfButton",
                              source_type());
    if (is_tablet_mode()) {
      UMA_HISTOGRAM_TIMES(
          "Apps.ContextMenuUserJourneyTimeV2.ShelfButton.TabletMode",
          user_journey_time);
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.ContextMenuShowSourceV2.ShelfButton.TabletMode", source_type());
    } else {
      UMA_HISTOGRAM_TIMES(
          "Apps.ContextMenuUserJourneyTimeV2.ShelfButton.ClamshellMode",
          user_journey_time);
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.ContextMenuShowSourceV2.ShelfButton.ClamshellMode",
          source_type());
    }
    return;
  }

  UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTimeV2.Shelf",
                      user_journey_time);
  UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSourceV2.Shelf",
                            source_type());
  if (is_tablet_mode()) {
    UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTimeV2.Shelf.TabletMode",
                        user_journey_time);
    UMA_HISTOGRAM_ENUMERATION("Apps.ContextMenuShowSourceV2.Shelf.TabletMode",
                              source_type());
  } else {
    UMA_HISTOGRAM_TIMES("Apps.ContextMenuUserJourneyTimeV2.Shelf.ClamshellMode",
                        user_journey_time);
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.ContextMenuShowSourceV2.Shelf.ClamshellMode", source_type());
  }
}

bool ShelfMenuModelAdapter::IsShowingMenuForView(
    const views::View& view) const {
  return IsShowingMenu() && menu_owner_ == &view;
}

}  // namespace ash
