// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"

#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_search_bar_textfield.h"
#include "base/functional/bind.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

void ApplyQuickInsertPseudoFocusToView(views::View* view) {
  if (view == nullptr) {
    return;
  }

  // QuickInsertItemView has special pseudo focus behavior, so handle it
  // separately.
  if (auto* item_view = views::AsViewClass<QuickInsertItemView>(view)) {
    item_view->SetItemState(QuickInsertItemView::ItemState::kPseudoFocused);
    if (auto* list_item_view =
            views::AsViewClass<QuickInsertListItemView>(view)) {
      list_item_view->SetBadgeVisible(true);
    }
    return;
  }

  // QuickInsertSearchBarTextfield has special pseudo focus appearance.
  if (auto* textfield =
          views::AsViewClass<QuickInsertSearchBarTextfield>(view)) {
    textfield->SetShouldShowFocusIndicator(true);
    return;
  }

  // Otherwise, default to drawing a focus ring around the view.
  if (views::FocusRing* focus_ring = views::FocusRing::Get(view)) {
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) { return true; }));
    focus_ring->SchedulePaint();
  }
}

void RemoveQuickInsertPseudoFocusFromView(views::View* view) {
  if (view == nullptr) {
    return;
  }

  // QuickInsertItemView has special pseudo focus behavior, so handle it
  // separately.
  if (auto* item_view = views::AsViewClass<QuickInsertItemView>(view)) {
    item_view->SetItemState(QuickInsertItemView::ItemState::kNormal);
    if (auto* list_item_view =
            views::AsViewClass<QuickInsertListItemView>(view)) {
      list_item_view->SetBadgeVisible(false);
    }
    return;
  }

  // QuickInsertSearchBarTextfield has special pseudo focus appearance.
  if (auto* textfield =
          views::AsViewClass<QuickInsertSearchBarTextfield>(view)) {
    textfield->SetShouldShowFocusIndicator(false);
    return;
  }

  // Otherwise, default to removing the focus ring around the view.
  if (views::FocusRing* focus_ring = views::FocusRing::Get(view)) {
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) { return false; }));
    focus_ring->SchedulePaint();
  }
}

bool DoQuickInsertPseudoFocusedActionOnView(views::View* view) {
  if (view == nullptr) {
    return false;
  }

  // QuickInsertSearchBarTextfield has no pseudo focus action.
  if (views::IsViewClass<QuickInsertSearchBarTextfield>(view)) {
    return true;
  }

  // QuickInsertItemView has special pseudo focus behavior, so handle it
  // separately.
  if (auto* item_view = views::AsViewClass<QuickInsertItemView>(view)) {
    item_view->SelectItem();
    return true;
  }

  // Otherwise, default to behaving the same way as pressing the enter key.
  // Here we check that `view` does not have actual focus, to ensure we won't
  // trigger an infinite recursive loop of pseudo focused actions when manually
  // calling `OnKeyEvent` (since the focused view may forward key events to be
  // handled by the pseudo focused view).
  CHECK(!view->HasFocus());
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                         ui::DomCode::ENTER, ui::EF_NONE);
  view->OnKeyEvent(&key_event);
  return key_event.handled();
}

views::View* GetNextQuickInsertPseudoFocusableView(
    views::View* view,
    QuickInsertPseudoFocusDirection direction,
    bool should_loop) {
  return view == nullptr || view->GetFocusManager() == nullptr
             ? nullptr
             : view->GetFocusManager()->GetNextFocusableView(
                   view, view->GetWidget(),
                   direction == QuickInsertPseudoFocusDirection::kBackward,
                   !should_loop);
}

}  // namespace ash
