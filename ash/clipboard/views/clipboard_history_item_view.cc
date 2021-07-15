// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"
#include "ash/clipboard/views/clipboard_history_delete_button.h"
#include "ash/clipboard/views/clipboard_history_file_item_view.h"
#include "ash/clipboard/views/clipboard_history_main_button.h"
#include "ash/clipboard/views/clipboard_history_text_item_view.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {
using Action = ClipboardHistoryUtil::Action;
}  // namespace

ClipboardHistoryItemView::ContentsView::ContentsView(
    ClipboardHistoryItemView* container)
    : container_(container) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  SetBorder(views::CreateEmptyBorder(ClipboardHistoryViews::kContentsInsets));
}

ClipboardHistoryItemView::ContentsView::~ContentsView() = default;

void ClipboardHistoryItemView::ContentsView::InstallDeleteButton() {
  delete_button_ = CreateDeleteButton();
}

void ClipboardHistoryItemView::ContentsView::OnHostPseudoFocusUpdated() {
  delete_button_->SetVisible(container_->ShouldShowDeleteButton());

  const bool focused =
      (container_->pseudo_focus_ == PseudoFocus::kDeleteButton);
  views::InkDrop::Get(delete_button_)->GetInkDrop()->SetFocused(focused);
  if (focused) {
    delete_button_->NotifyAccessibilityEvent(ax::mojom::Event::kHover,
                                             /*send_native_event*/ true);
  }
}

const char* ClipboardHistoryItemView::ContentsView::GetClassName() const {
  return "ContenstView";
}

// Accepts the event only when |delete_button_| should be the handler.
bool ClipboardHistoryItemView::ContentsView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  if (!delete_button_->GetVisible())
    return false;

  gfx::RectF rect_in_delete_button(rect);
  ConvertRectToTarget(this, delete_button_, &rect_in_delete_button);
  return delete_button_->HitTestRect(
      gfx::ToEnclosedRect(rect_in_delete_button));
}

// static
std::unique_ptr<ClipboardHistoryItemView>
ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
    const ClipboardHistoryItem& item,
    const ClipboardHistoryResourceManager* resource_manager,
    views::MenuItemView* container) {
  const auto display_format =
      ClipboardHistoryUtil::CalculateDisplayFormat(item.data());
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", display_format);
  switch (display_format) {
    case ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kText:
      return std::make_unique<ClipboardHistoryTextItemView>(&item, container);
    case ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kPng:
    case ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kHtml:
      return std::make_unique<ClipboardHistoryBitmapItemView>(
          &item, resource_manager, container);
    case ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kFile:
      return std::make_unique<ClipboardHistoryFileItemView>(&item, container);
  }
}

ClipboardHistoryItemView::~ClipboardHistoryItemView() = default;

ClipboardHistoryItemView::ClipboardHistoryItemView(
    const ClipboardHistoryItem* clipboard_history_item,
    views::MenuItemView* container)
    : clipboard_history_item_(clipboard_history_item), container_(container) {}

bool ClipboardHistoryItemView::AdvancePseudoFocus(bool reverse) {
  if (pseudo_focus_ == PseudoFocus::kEmpty) {
    InitiatePseudoFocus(reverse);
    return true;
  }

  // When the menu item is disabled, only the delete button is able to work.
  if (!container_->GetEnabled()) {
    DCHECK_EQ(PseudoFocus::kDeleteButton, pseudo_focus_);
    SetPseudoFocus(PseudoFocus::kEmpty);
    return false;
  }

  DCHECK(pseudo_focus_ == PseudoFocus::kMainButton ||
         pseudo_focus_ == PseudoFocus::kDeleteButton);
  int new_pseudo_focus = pseudo_focus_;
  bool move_focus_out = false;
  if (reverse) {
    --new_pseudo_focus;
    if (new_pseudo_focus == PseudoFocus::kEmpty)
      move_focus_out = true;
  } else {
    ++new_pseudo_focus;
    if (new_pseudo_focus == PseudoFocus::kMaxValue)
      move_focus_out = true;
  }

  if (move_focus_out) {
    SetPseudoFocus(PseudoFocus::kEmpty);
    return false;
  }

  SetPseudoFocus(static_cast<PseudoFocus>(new_pseudo_focus));
  return true;
}

void ClipboardHistoryItemView::HandleDeleteButtonPressEvent(
    const ui::Event& event) {
  Activate(Action::kDelete, event.flags());
}

void ClipboardHistoryItemView::HandleMainButtonPressEvent(
    const ui::Event& event) {
  // Note that the callback may be triggered through the ENTER key when
  // the delete button is under the pseudo focus. Because the delete
  // button is not hot-tracked by the menu controller. Meanwhile, the menu
  // controller always sends the key event to the hot-tracked view.
  // TODO(https://crbug.com/1144994): Modify this part after the clipboard
  // history menu code is refactored.

  // When an item view is under gesture tap, it may be not under pseudo
  // focus yet.
  if (event.type() == ui::ET_GESTURE_TAP)
    pseudo_focus_ = PseudoFocus::kMainButton;

  Activate(CalculateActionForMainButtonClick(), event.flags());
}

void ClipboardHistoryItemView::Init() {
  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenuItem);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Ensures that MainButton is below any other child views.
  main_button_ =
      AddChildView(std::make_unique<ClipboardHistoryMainButton>(this));

  contents_view_ = AddChildView(CreateContentsView());

  subscription_ = container_->AddSelectedChangedCallback(base::BindRepeating(
      &ClipboardHistoryItemView::OnSelectionChanged, base::Unretained(this)));
}

void ClipboardHistoryItemView::MaybeHandleGestureEventFromMainButton(
    ui::GestureEvent* event) {
  // `event` is always handled here if the menu item view is under the gesture
  // long press. It prevents other event handlers from introducing side effects.
  // For example, if `main_button_` handles the ui::ET_GESTURE_END event,
  // `main_button_`'s state will be reset. However, `main_button_` is expected
  // to be at the "hovered" state when the menu item is selected.
  if (under_gesture_long_press_) {
    DCHECK_NE(ui::ET_GESTURE_LONG_PRESS, event->type());
    if (event->type() == ui::ET_GESTURE_END)
      under_gesture_long_press_ = false;

    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    under_gesture_long_press_ = true;
    switch (pseudo_focus_) {
      case PseudoFocus::kEmpty:
        // Select the menu item if it is not selected yet.
        Activate(Action::kSelect, event->flags());
        break;
      case PseudoFocus::kMainButton: {
        // The menu item is already selected so show the delete button if the
        // button is hidden.
        views::View* delete_button = contents_view_->delete_button();
        if (!delete_button->GetVisible())
          delete_button->SetVisible(true);
        break;
      }
      case PseudoFocus::kDeleteButton:
        // The delete button already shows, so do nothing.
        DCHECK(contents_view_->delete_button()->GetVisible());
        break;
      case PseudoFocus::kMaxValue:
        NOTREACHED();
        break;
    }
    event->SetHandled();
  }
}

void ClipboardHistoryItemView::OnSelectionChanged() {
  if (!container_->IsSelected()) {
    SetPseudoFocus(PseudoFocus::kEmpty);
    return;
  }

  // If the pseudo focus is moved from another item view via focus traversal,
  // `pseudo_focus_` is already up to date.
  if (pseudo_focus_ != PseudoFocus::kEmpty)
    return;

  InitiatePseudoFocus(/*reverse=*/false);
}

bool ClipboardHistoryItemView::ShouldHighlight() const {
  return pseudo_focus_ == PseudoFocus::kMainButton;
}

void ClipboardHistoryItemView::OnMouseClickOnDescendantCanceled() {
  // When mouse click is canceled, mouse may hover a different menu item from
  // the one where the click event started. A typical way is to move the mouse
  // while pressing the mouse left button. Hence, update the menu selection due
  // to the mouse location change.
  Activate(ClipboardHistoryUtil::Action::kSelectItemHoveredByMouse,
           ui::EF_NONE);
}

void ClipboardHistoryItemView::MaybeRecordButtonPressedHistogram() const {
  switch (action_) {
    case Action::kDelete:
      ClipboardHistoryUtil::RecordClipboardHistoryItemDeleted(
          *clipboard_history_item_);
      return;
    case Action::kPaste:
      ClipboardHistoryUtil::RecordClipboardHistoryItemPasted(
          *clipboard_history_item_);
      return;
    case Action::kSelect:
    case Action::kSelectItemHoveredByMouse:
      return;
    case Action::kEmpty:
      NOTREACHED();
      return;
  }
}

gfx::Size ClipboardHistoryItemView::CalculatePreferredSize() const {
  const int preferred_width =
      views::MenuConfig::instance().touchable_menu_min_width;
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

void ClipboardHistoryItemView::GetAccessibleNodeData(ui::AXNodeData* data) {
  data->SetName(GetAccessibleName());
}

void ClipboardHistoryItemView::Activate(Action action, int event_flags) {
  DCHECK_EQ(Action::kEmpty, action_);
  DCHECK_NE(action_, action);

  base::AutoReset<Action> action_to_take(&action_, action);
  MaybeRecordButtonPressedHistogram();

  views::MenuDelegate* delegate = container_->GetDelegate();
  const int command_id = container_->GetCommand();
  DCHECK(delegate->IsCommandEnabled(command_id));
  delegate->ExecuteCommand(command_id, event_flags);
}

Action ClipboardHistoryItemView::CalculateActionForMainButtonClick() const {
  // `main_button_` may be clicked when the delete button is under the pseudo
  // focus. It happens when a user presses the ENTER key. Note that the menu
  // controller sends the accelerator to the hot-tracked view and `main_button_`
  // is hot-tracked when the delete button is under the pseudo focus. The menu
  // controller should not hot-track the delete button. Otherwise, pressing the
  // up/down arrow key will select a delete button instead of a neighboring
  // menu item.

  switch (pseudo_focus_) {
    case PseudoFocus::kMainButton:
      return Action::kPaste;
    case PseudoFocus::kDeleteButton:
      return Action::kDelete;
    case PseudoFocus::kEmpty:
    case PseudoFocus::kMaxValue:
      NOTREACHED();
      return Action::kEmpty;
  }
}

bool ClipboardHistoryItemView::ShouldShowDeleteButton() const {
  return (pseudo_focus_ == PseudoFocus::kMainButton && IsMouseHovered()) ||
         pseudo_focus_ == PseudoFocus::kDeleteButton ||
         under_gesture_long_press_;
}

void ClipboardHistoryItemView::InitiatePseudoFocus(bool reverse) {
  PseudoFocus target_pseudo_focus;
  if (!container_->GetEnabled() || reverse)
    target_pseudo_focus = PseudoFocus::kDeleteButton;
  else
    target_pseudo_focus = PseudoFocus::kMainButton;

  SetPseudoFocus(target_pseudo_focus);
}

void ClipboardHistoryItemView::SetPseudoFocus(PseudoFocus new_pseudo_focus) {
  DCHECK_NE(PseudoFocus::kMaxValue, new_pseudo_focus);
  if (pseudo_focus_ == new_pseudo_focus)
    return;

  pseudo_focus_ = new_pseudo_focus;
  if (pseudo_focus_ == PseudoFocus::kMainButton) {
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection,
                             /*send_native_event=*/true);
  }

  contents_view_->OnHostPseudoFocusUpdated();
  main_button_->OnHostPseudoFocusUpdated();
}

}  // namespace ash
