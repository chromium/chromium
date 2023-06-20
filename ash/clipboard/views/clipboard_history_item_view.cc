// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_item_view.h"

#include <memory>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"
#include "ash/clipboard/views/clipboard_history_delete_button.h"
#include "ash/clipboard/views/clipboard_history_main_button.h"
#include "ash/clipboard/views/clipboard_history_text_item_view.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {
namespace {
using Action = clipboard_history_util::Action;

const ClipboardHistoryItem* GetClipboardHistoryItemImpl(
    const base::UnguessableToken& item_id,
    const ClipboardHistory* clipboard_history) {
  const auto& items = clipboard_history->GetItems();
  const auto& item_iter =
      base::ranges::find(items, item_id, &ClipboardHistoryItem::id);
  return item_iter == items.cend() ? nullptr : &(*item_iter);
}

const gfx::Insets GetDeleteButtonMargins(
    crosapi::mojom::ClipboardHistoryDisplayFormat display_format) {
  switch (display_format) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kUnknown:
      NOTREACHED_NORETURN();
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
      return ClipboardHistoryViews::kTextItemDeleteButtonMargins;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
      return ClipboardHistoryViews::kBitmapItemDeleteButtonMargins;
  }
}
}  // namespace

// Container class for everything that visibly appears in a menu item.
class ClipboardHistoryItemView::DisplayView
    : public views::BoxLayoutView,
      public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(DisplayView);
  explicit DisplayView(ClipboardHistoryItemView* container)
      : container_(container) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
    SetBorder(views::CreateEmptyBorder(ClipboardHistoryViews::kContentsInsets));
  }

  DisplayView(const DisplayView& rhs) = delete;
  DisplayView& operator=(const DisplayView& rhs) = delete;

  ~DisplayView() override = default;

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    const views::View* const delete_button = container_->delete_button_;
    if (!delete_button->GetVisible()) {
      return false;
    }

    gfx::RectF rect_in_delete_button(rect);
    ConvertRectToTarget(this, delete_button, &rect_in_delete_button);
    return delete_button->HitTestRect(
        gfx::ToEnclosedRect(rect_in_delete_button));
  }

  // The parent item view.
  const raw_ptr<ClipboardHistoryItemView> container_;
};

// static
std::unique_ptr<ClipboardHistoryItemView>
ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
    const base::UnguessableToken& item_id,
    const ClipboardHistory* clipboard_history,
    views::MenuItemView* container) {
  const auto* item = GetClipboardHistoryItemImpl(item_id, clipboard_history);
  const auto display_format = item->display_format();
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatShown", display_format);
  switch (display_format) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kUnknown:
      NOTREACHED_NORETURN();
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
      return std::make_unique<ClipboardHistoryTextItemView>(
          item_id, clipboard_history, container);
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
      return std::make_unique<ClipboardHistoryBitmapItemView>(
          item_id, clipboard_history, container);
  }
}

ClipboardHistoryItemView::~ClipboardHistoryItemView() = default;

ClipboardHistoryItemView::ClipboardHistoryItemView(
    const base::UnguessableToken& item_id,
    const ClipboardHistory* clipboard_history,
    views::MenuItemView* container)
    : item_id_(item_id),
      clipboard_history_(clipboard_history),
      container_(container) {}

bool ClipboardHistoryItemView::AdvancePseudoFocus(bool reverse) {
  if (pseudo_focus_ == PseudoFocus::kEmpty) {
    InitiatePseudoFocus(reverse);
    return true;
  }

  // When the menu item is disabled, only the delete button is able to work.
  if (!container_->GetEnabled()) {
    DCHECK(IsDeleteButtonPseudoFocused());
    SetPseudoFocus(PseudoFocus::kEmpty);
    return false;
  }

  DCHECK(IsMainButtonPseudoFocused() || IsDeleteButtonPseudoFocused());
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
  views::BoxLayoutView* display_view = nullptr;
  views::Builder<views::View>(this)
      .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
      .SetLayoutManager(std::make_unique<views::FillLayout>())
      .AddChildren(
          // Add the main button below the delete button in the z-order so that
          // hovering over the delete button causes it to be recognized as the
          // item view's event handler.
          views::Builder<views::View>(
              std::make_unique<ClipboardHistoryMainButton>(this))
              .CopyAddressTo(&main_button_),
          views::Builder<views::BoxLayoutView>(
              std::make_unique<DisplayView>(this))
              .CopyAddressTo(&display_view)
              .AddChild(views::Builder<views::View>(CreateContentsView())
                            .AddChild(views::Builder<views::View>(
                                CreateDeleteButton()))))
      .BuildChildren();

  const auto* const item = GetClipboardHistoryItem();
  CHECK(item);
  if (item->display_format() ==
      crosapi::mojom::ClipboardHistoryDisplayFormat::kFile) {
    CHECK(item->icon());
    views::Builder<views::View>(display_view)
        .AddChildAt(views::Builder<views::ImageView>()
                        .SetImageSize(ClipboardHistoryViews::kIconSize)
                        .SetProperty(views::kMarginsKey,
                                     ClipboardHistoryViews::kIconMargins)
                        .SetImage(*item->icon()),
                    /*index=*/0)
        .BuildChildren();
  }

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
        if (!delete_button_->GetVisible()) {
          delete_button_->SetVisible(true);
        }
        break;
      }
      case PseudoFocus::kDeleteButton:
        // The delete button already shows, so do nothing.
        DCHECK(delete_button_->GetVisible());
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

bool ClipboardHistoryItemView::IsMainButtonPseudoFocused() const {
  return pseudo_focus_ == PseudoFocus::kMainButton;
}

bool ClipboardHistoryItemView::IsDeleteButtonPseudoFocused() const {
  return pseudo_focus_ == PseudoFocus::kDeleteButton;
}

void ClipboardHistoryItemView::OnMouseClickOnDescendantCanceled() {
  // When mouse click is canceled, mouse may hover a different menu item from
  // the one where the click event started. A typical way is to move the mouse
  // while pressing the mouse left button. Hence, update the menu selection due
  // to the mouse location change.
  Activate(Action::kSelectItemHoveredByMouse, ui::EF_NONE);
}

const ClipboardHistoryItem* ClipboardHistoryItemView::GetClipboardHistoryItem()
    const {
  return GetClipboardHistoryItemImpl(item_id_, clipboard_history_);
}

gfx::Size ClipboardHistoryItemView::CalculatePreferredSize() const {
  const int preferred_width =
      clipboard_history_util::GetPreferredItemViewWidth();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

void ClipboardHistoryItemView::GetAccessibleNodeData(ui::AXNodeData* data) {
  // A valid role must be set in the AXNodeData prior to setting the name
  // via AXNodeData::SetName.
  data->role = ax::mojom::Role::kMenuItem;
  data->SetNameChecked(GetAccessibleName());

  // In fitting with existing conventions for menu items, we treat clipboard
  // history items as "selected" from an accessibility standpoint if pressing
  // Enter will perform the item's default expected action: pasting.
  data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                         IsMainButtonPseudoFocused());
}

void ClipboardHistoryItemView::Activate(Action action, int event_flags) {
  DCHECK_EQ(Action::kEmpty, action_);
  DCHECK_NE(action_, action);

  base::AutoReset<Action> action_to_take(&action_, action);

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

std::unique_ptr<views::View> ClipboardHistoryItemView::CreateDeleteButton() {
  const auto* const item = GetClipboardHistoryItem();
  CHECK(item);

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
      .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
      .AddChild(views::Builder<views::Button>(
                    std::make_unique<ClipboardHistoryDeleteButton>(this))
                    .SetProperty(views::kMarginsKey,
                                 GetDeleteButtonMargins(item->display_format()))
                    .CopyAddressTo(&delete_button_))
      .Build();
}

bool ClipboardHistoryItemView::ShouldShowDeleteButton() const {
  return (IsMainButtonPseudoFocused() && IsMouseHovered()) ||
         IsDeleteButtonPseudoFocused() || under_gesture_long_press_;
}

void ClipboardHistoryItemView::InitiatePseudoFocus(bool reverse) {
  SetPseudoFocus(reverse || !container_->GetEnabled()
                     ? PseudoFocus::kDeleteButton
                     : PseudoFocus::kMainButton);
}

void ClipboardHistoryItemView::SetPseudoFocus(PseudoFocus new_pseudo_focus) {
  DCHECK_NE(PseudoFocus::kMaxValue, new_pseudo_focus);
  if (pseudo_focus_ == new_pseudo_focus)
    return;

  // The main button appears highlighted when it has pseudo focus. The button
  // needs to be repainted when transitioning to or from a highlighted state.
  const bool repaint_main_button = pseudo_focus_ == PseudoFocus::kMainButton ||
                                   new_pseudo_focus == PseudoFocus::kMainButton;

  pseudo_focus_ = new_pseudo_focus;
  if (IsMainButtonPseudoFocused()) {
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection,
                             /*send_native_event=*/true);
  }

  delete_button_->SetVisible(ShouldShowDeleteButton());
  views::InkDrop::Get(delete_button_)
      ->GetInkDrop()
      ->SetFocused(IsDeleteButtonPseudoFocused());
  if (IsDeleteButtonPseudoFocused()) {
    delete_button_->NotifyAccessibilityEvent(ax::mojom::Event::kHover,
                                             /*send_native_event*/ true);
  }

  if (repaint_main_button) {
    main_button_->SchedulePaint();
  }
}

BEGIN_METADATA(ClipboardHistoryItemView, DisplayView, views::View)
END_METADATA

BEGIN_METADATA(ClipboardHistoryItemView, views::View)
END_METADATA

}  // namespace ash
