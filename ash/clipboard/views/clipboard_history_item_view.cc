// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_item_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"
#include "ash/clipboard/views/clipboard_history_delete_button.h"
#include "ash/clipboard/views/clipboard_history_main_button.h"
#include "ash/clipboard/views/clipboard_history_text_item_view.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/style/typography.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/view_utils.h"

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
  // When the refresh is enabled, delete buttons are fully top-right aligned.
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    return gfx::Insets();
  }

  switch (display_format) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kUnknown:
      NOTREACHED();
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
      return ClipboardHistoryViews::kTextItemDeleteButtonMargins;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
      return ClipboardHistoryViews::kBitmapItemDeleteButtonMargins;
  }
}

// Creates a label with the text "Ctrl+V" to be displayed under the contents of
// the first item in the clipboard history menu.
std::unique_ptr<views::Label> CreateCtrlVLabel() {
  return views::Builder<views::Label>(
             bubble_utils::CreateLabel(
                 TypographyToken::kCrosLabel1,
                 ui::Accelerator(ui::VKEY_V, ui::EF_CONTROL_DOWN)
                     .GetShortcutText(),
                 cros_tokens::kCrosSysSecondary))
      .SetID(clipboard_history_util::kCtrlVLabelID)
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .Build();
}
}  // namespace

// ClipboardHistoryItemView::ContentsView --------------------------------------

ClipboardHistoryItemView::ContentsView::ContentsView() {
  SetID(clipboard_history_util::kContentsViewID);
}

ClipboardHistoryItemView::ContentsView::~ContentsView() = default;

void ClipboardHistoryItemView::ContentsView::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  is_delete_button_visible_ = observed_view->GetVisible();
  SetClipPath(GetClipPath());
}

// ClipboardHistoryItemView::DisplayView ---------------------------------------

// Container class for everything that visibly appears in a menu item.
class ClipboardHistoryItemView::DisplayView
    : public views::BoxLayoutView,
      public views::ViewTargeterDelegate {
  METADATA_HEADER(DisplayView, views::BoxLayoutView)

 public:
  explicit DisplayView(ClipboardHistoryItemView* container)
      : container_(container) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
    SetBorder(views::CreateEmptyBorder(ClipboardHistoryViews::kContentsInsets));

    // Add an icon portraying the item's type if `this` is meant to display one.
    const auto* const item = container->GetClipboardHistoryItem();
    CHECK(item);
    if (item->display_format() ==
            crosapi::mojom::ClipboardHistoryDisplayFormat::kFile ||
        chromeos::features::IsClipboardHistoryRefreshEnabled()) {
      CHECK(item->icon());
      AddChildView(views::Builder<views::ImageView>()
                       .SetImageSize(ClipboardHistoryViews::kIconSize)
                       .SetProperty(views::kMarginsKey,
                                    ClipboardHistoryViews::kIconMargins)
                       .SetImage(*item->icon())
                       .Build());
    }

    if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
      // Add the item's contents and a delete button occupying the same space.
      AddChildView(
          views::Builder<views::View>()
              .SetLayoutManager(std::make_unique<views::FillLayout>())
              .SetProperty(views::kBoxLayoutFlexKey,
                           views::BoxLayoutFlexSpecification())
              .AddChildren(
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(views::BoxLayout::Orientation::kVertical)
                      .SetBetweenChildSpacing(
                          ClipboardHistoryViews::kCtrlVLabelPadding)
                      .AddChildren(
                          views::Builder<views::View>(
                              container->CreateContentsView())
                              .CopyAddressTo(&contents_view_),
                          views::Builder<views::Label>(CreateCtrlVLabel())
                              .CopyAddressTo(&container->ctrl_v_label_)
                              // The Ctrl+V label is hidden by default.
                              // `ShowCtrlVLabel()` will be called on the menu's
                              // first item to make its label visible.
                              .SetVisible(false)),
                  views::Builder<views::View>(container->CreateDeleteButton()))
              .Build());

      // `CreateDeleteButton()` already calls `CopyAddressTo()` when building
      // the delete button, so it will not copy the right address if called
      // again. Therefore, we cache `delete_button_` outside of the builder.
      delete_button_ = container->delete_button_.get();

      // `contents_view_` observes `delete_button_` so that the former can be
      // clipped to avoid overlapping with the latter.
      delete_button_->AddObserver(
          views::AsViewClass<ContentsView>(contents_view_));
    } else {
      // Add the item's contents and a delete button that, when visible, takes
      // away some of the contents' horizontal space.
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::LayoutOrientation::kVertical)
                       .AddChild(views::Builder<views::View>(
                                     container->CreateContentsView())
                                     .AddChild(views::Builder<views::View>(
                                         container->CreateDeleteButton())))
                       .Build());
    }
  }

  DisplayView(const DisplayView& rhs) = delete;
  DisplayView& operator=(const DisplayView& rhs) = delete;

  ~DisplayView() override {
    if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
      delete_button_->RemoveObserver(
          views::AsViewClass<ContentsView>(contents_view_));
    }
  }

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

  // Owned by the view hierarchy. Only set when the clipboard history refresh is
  // enabled.
  raw_ptr<views::View> contents_view_;

  // Owned by the view hierarchy. Only set when the clipboard history refresh is
  // enabled. Cached locally because `container_` cannot be accessed when `this`
  // is being destroyed.
  raw_ptr<views::View> delete_button_;
};

// ClipboardHistoryItemView ----------------------------------------------------

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

  std::unique_ptr<ClipboardHistoryItemView> item_view;
  switch (display_format) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kUnknown:
      NOTREACHED();
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
      item_view = std::make_unique<ClipboardHistoryTextItemView>(
          item_id, clipboard_history, container);
      break;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
      item_view = std::make_unique<ClipboardHistoryBitmapItemView>(
          item_id, clipboard_history, container);
      break;
  }
  // Initialize `item_view` now that it can create format-specific contents.
  item_view->Init();
  return item_view;
}

ClipboardHistoryItemView::~ClipboardHistoryItemView() = default;

ClipboardHistoryItemView::ClipboardHistoryItemView(
    const base::UnguessableToken& item_id,
    const ClipboardHistory* clipboard_history,
    views::MenuItemView* container)
    : item_id_(item_id),
      clipboard_history_(clipboard_history),
      container_(container) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
}

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
  if (event.type() == ui::EventType::kGestureTap) {
    pseudo_focus_ = PseudoFocus::kMainButton;
    UpdateAccessiblitySelectionAttribute();
  }

  Activate(CalculateActionForMainButtonClick(), event.flags());
}

void ClipboardHistoryItemView::ShowCtrlVLabel() {
  CHECK(ctrl_v_label_);
  ctrl_v_label_->SetVisible(true);
}

void ClipboardHistoryItemView::MaybeHandleGestureEventFromMainButton(
    ui::GestureEvent* event) {
  // `event` is always handled here if the menu item view is under the gesture
  // long press. It prevents other event handlers from introducing side effects.
  // For example, if `main_button_` handles the ui::EventType::kGestureEnd
  // event, `main_button_`'s state will be reset. However, `main_button_` is
  // expected to be at the "hovered" state when the menu item is selected.
  if (under_gesture_long_press_) {
    DCHECK_NE(ui::EventType::kGestureLongPress, event->type());
    if (event->type() == ui::EventType::kGestureEnd) {
      under_gesture_long_press_ = false;
    }

    event->SetHandled();
    return;
  }

  if (event->type() == ui::EventType::kGestureLongPress) {
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

gfx::Size ClipboardHistoryItemView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_width =
      clipboard_history_util::GetPreferredItemViewWidth();
  return gfx::Size(
      preferred_width,
      GetLayoutManager()->GetPreferredHeightForWidth(this, preferred_width));
}

void ClipboardHistoryItemView::Init() {
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
          views::Builder<views::View>(std::make_unique<DisplayView>(this)))
      .BuildChildren();

  subscription_ = container_->AddSelectedChangedCallback(base::BindRepeating(
      &ClipboardHistoryItemView::OnSelectionChanged, base::Unretained(this)));
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
      DUMP_WILL_BE_NOTREACHED();
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
                    std::make_unique<ClipboardHistoryDeleteButton>(
                        this, item->display_text()))
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
  UpdateAccessiblitySelectionAttribute();

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

void ClipboardHistoryItemView::UpdateAccessiblitySelectionAttribute() {
  // In fitting with existing conventions for menu items, we treat clipboard
  // history items as "selected" from an accessibility standpoint if pressing
  // Enter will perform the item's default expected action: pasting.
  GetViewAccessibility().SetIsSelected(IsMainButtonPseudoFocused());
}

BEGIN_METADATA(ClipboardHistoryItemView, ContentsView)
END_METADATA

BEGIN_METADATA(ClipboardHistoryItemView, DisplayView)
END_METADATA

BEGIN_METADATA(ClipboardHistoryItemView)
END_METADATA

}  // namespace ash
