// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_pseudo_focus.h"

#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "base/functional/bind.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

void ApplyPickerPseudoFocusToView(views::View* view) {
  if (view == nullptr) {
    return;
  }

  // PickerItemView has special pseudo focus behavior, so handle it separately.
  if (views::IsViewClass<PickerItemView>(view)) {
    views::AsViewClass<PickerItemView>(view)->SetItemState(
        PickerItemView::ItemState::kPseudoFocused);
    if (views::IsViewClass<PickerListItemView>(view)) {
      views::AsViewClass<PickerListItemView>(view)->SetBadgeVisible(true);
    }
    return;
  }

  // Otherwise, default to drawing a focus ring around the view.
  // TODO: b/328144222 - Add accessibility announcement when a view gains
  // pseudo focus.
  if (views::FocusRing* focus_ring = views::FocusRing::Get(view)) {
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) { return true; }));
    focus_ring->SchedulePaint();
  }
}

void RemovePickerPseudoFocusFromView(views::View* view) {
  if (view == nullptr) {
    return;
  }

  // PickerItemView has special pseudo focus behavior, so handle it separately.
  if (views::IsViewClass<PickerItemView>(view)) {
    views::AsViewClass<PickerItemView>(view)->SetItemState(
        PickerItemView::ItemState::kNormal);
    if (views::IsViewClass<PickerListItemView>(view)) {
      views::AsViewClass<PickerListItemView>(view)->SetBadgeVisible(false);
    }
    return;
  }

  // Otherwise, default to removing the focus ring around the view.
  // TODO: b/328144222 - Add accessibility announcement when a view loses
  // pseudo focus.
  if (views::FocusRing* focus_ring = views::FocusRing::Get(view)) {
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) { return false; }));
    focus_ring->SchedulePaint();
  }
}

bool DoPickerPseudoFocusedActionOnView(views::View* view) {
  if (view == nullptr) {
    return false;
  }

  // PickerItemView has special pseudo focus behavior, so handle it separately.
  if (views::IsViewClass<PickerItemView>(view)) {
    views::AsViewClass<PickerItemView>(view)->SelectItem();
    return true;
  }

  // Otherwise, default to behaving the same way as pressing the enter key.
  // Here we check that `view` does not have actual focus, to ensure we won't
  // trigger an infinite recursive loop of pseudo focused actions when manually
  // calling `OnKeyEvent` (since the focused view may forward key events to be
  // handled by the pseudo focused view).
  CHECK(!view->HasFocus());
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RETURN,
                         ui::DomCode::ENTER, ui::EF_NONE);
  view->OnKeyEvent(&key_event);
  return key_event.handled();
}

}  // namespace ash
