// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/grit/component_extension_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace arc::input_overlay {

namespace {

constexpr int kMainContainerWidth = 296;

constexpr int kHeaderBottomMargin = 16;
constexpr int kAddRowBottomMargin = 8;
constexpr int kAddButtonCornerRadius = 10;
// This is associated to the size of `ash::IconButton::Type::kMedium`.
constexpr int kIconButtonSize = 32;

// Gap from focus ring outer edge to the edge of the view.
constexpr float kHaloInset = -4.0f;
// Thickness of focus ring.
constexpr float kHaloThickness = 2.0f;

constexpr size_t kMaxActionCount = 50;

}  // namespace

EditingList::EditingList(DisplayOverlayController* controller)
    : TouchInjectorObserver(), controller_(controller) {
  controller_->AddTouchInjectorObserver(this);
  Init();
}

EditingList::~EditingList() {
  controller_->RemoveTouchInjectorObserver(this);
}

void EditingList::UpdateWidget() {
  auto* widget = GetWidget();
  DCHECK(widget);

  controller_->UpdateWidgetBoundsInRootWindow(
      widget, gfx::Rect(GetWidgetMagneticPositionLocal(), GetPreferredSize()));
}

void EditingList::ShowEduNudgeForEditingTip() {
  DCHECK_EQ(scroll_content_->children().size(), 1u);
  DCHECK(!is_zero_state_);

  static_cast<ActionViewListItem*>(scroll_content_->children()[0])
      ->ShowEduNudgeForEditingTip();
}

bool EditingList::OnMousePressed(const ui::MouseEvent& event) {
  OnDragStart(event);
  return true;
}

bool EditingList::OnMouseDragged(const ui::MouseEvent& event) {
  OnDragUpdate(event);
  return true;
}

void EditingList::OnMouseReleased(const ui::MouseEvent& event) {
  OnDragEnd(event);
}

void EditingList::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      OnDragStart(*event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      OnDragUpdate(*event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      OnDragEnd(*event);
      event->SetHandled();
      break;
    default:
      return;
  }
}

void EditingList::Init() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque, /*radius=*/24));
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kEditingListInsideBorderInsets, 0)));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  AddHeader();
  AddActionAddRow();

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetBackgroundColor(absl::nullopt);
  on_scroll_view_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &EditingList::OnScrollViewScrolled, base::Unretained(this)));
  scroll_content_ = scroll_view_->SetContents(std::make_unique<views::View>());
  scroll_content_
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          /*inside_border_insets=*/gfx::Insets(),
          /*between_child_spacing=*/8))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  scroll_content_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(0, kEditingListInsideBorderInsets)));

  // Add contents.
  if (HasControls()) {
    AddControlListContent();
  } else {
    UpdateOnZeroState(/*is_zero_state=*/true);
  }

  SizeToPreferredSize();
}

bool EditingList::HasControls() const {
  DCHECK(controller_);
  return controller_->GetActiveActionsSize() != 0u;
}

void EditingList::AddHeader() {
  // +-----------------------------------+
  // ||"Controls"|    |? button| |"Done"||
  // +-----------------------------------+
  auto* header_container =
      AddChildView(std::make_unique<views::TableLayoutView>());
  header_container
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kStart,
                  /*v_align=*/views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/views::TableLayout::kFixedSize,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/32)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kEnd,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/8)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kEnd,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/views::TableLayout::kFixedSize,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, views::TableLayout::kFixedSize);
  header_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, kEditingListInsideBorderInsets, kHeaderBottomMargin,
                        kEditingListInsideBorderInsets));

  // Add header title.
  editing_header_label_ =
      header_container->AddChildView(ash::bubble_utils::CreateLabel(
          ash::TypographyToken::kCrosTitle1,
          // TODO(b/274690042): Replace it with localized strings.
          u"Controls", cros_tokens::kCrosSysOnSurface));

  // Add helper button.
  header_container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&EditingList::OnHelpButtonPressed,
                          base::Unretained(this)),
      // TODO(b/296126993): Add the UX provided back arrow icon.
      ash::IconButton::Type::kMedium, &ash::kGdHelpIcon,
      // TODO(b/279117180): Update a11y string.
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));

  // Add done button.
  header_container->AddChildView(std::make_unique<ash::PillButton>(
      base::BindRepeating(&EditingList::OnDoneButtonPressed,
                          base::Unretained(this)),
      // TODO(b/274690042): Replace it with localized strings.
      u"Done", ash::PillButton::Type::kSecondaryWithoutIcon));
}

void EditingList::AddActionAddRow() {
  // +-----------------------------------+
  // ||"Create (your first) button"|  |+||
  // +-----------------------------------+
  add_container_ = AddChildView(std::make_unique<views::TableLayoutView>());
  add_container_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(14, 16)));
  UpdateAddContainerBackground(/*add_background=*/true);
  add_container_
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kStart,
                  /*v_align=*/views::LayoutAlignment::kStart,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/12)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kEnd,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);

  // Add title for `add_container_`.
  add_title_ = add_container_->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosButton2, u"", cros_tokens::kCrosSysOnSurface));

  // Add `add_button_` and apply design style.
  add_button_ = add_container_->AddChildView(
      std::make_unique<views::LabelButton>(base::BindRepeating(
          &EditingList::OnAddButtonPressed, base::Unretained(this))));
  // TODO(b/274690042): Replace it with localized strings.
  add_button_->SetAccessibleName(u"add");
  add_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysPrimary, /*radius=*/kAddButtonCornerRadius));
  add_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kGameControlsAddIcon,
                                     cros_tokens::kCrosSysOnPrimary));
  add_button_->SetImageCentered(true);

  views::HighlightPathGenerator::Install(
      add_button_,
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(), /*corner_radius*/ kAddButtonCornerRadius));
}

void EditingList::AddControlListContent() {
  UpdateOnZeroState(/*is_zero_state=*/false);

  // Add list content as:
  // --------------------------
  // | ---------------------- |
  // | | ActionViewListItem | |
  // | ---------------------- |
  // | ---------------------- |
  // | | ActionViewListItem | |
  // | ---------------------- |
  // | ......                 |
  // --------------------------
  // TODO(b/270969479): Wrap `scroll_content` in a scroll view.
  DCHECK(controller_);
  DCHECK(scroll_content_);
  for (const auto& action : controller_->touch_injector()->actions()) {
    if (action->IsDeleted()) {
      continue;
    }
    scroll_content_->AddChildView(
        std::make_unique<ActionViewListItem>(controller_, action.get()));
  }
}

void EditingList::UpdateOnZeroState(bool is_zero_state) {
  is_zero_state_ = is_zero_state;

  DCHECK(add_container_);
  add_container_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, kEditingListInsideBorderInsets,
                        is_zero_state_ ? 0 : kAddRowBottomMargin,
                        kEditingListInsideBorderInsets));

  DCHECK(add_title_);
  // TODO(b/274690042): Replace it with localized strings.
  add_title_->SetText(is_zero_state_ ? u"Create your first button"
                                     : u"Create button");
}

void EditingList::OnAddButtonPressed() {
  // TODO(b/304819827): Support action type choose.
  controller_->EnterButtonPlaceMode(ActionType::TAP);
}

void EditingList::OnDoneButtonPressed() {
  DCHECK(controller_);
  controller_->OnCustomizeSave();
}

void EditingList::OnHelpButtonPressed() {
  // TODO(b/304852280)： Implement the function for helper button.
  NOTIMPLEMENTED();
}

void EditingList::UpdateAddButtonState() {
  add_button_->SetEnabled(controller_->GetActiveActionsSize() <
                          kMaxActionCount);
}

void EditingList::UpdateAddContainerBackground(bool add_background) {
  // No need to update the background if there is an expected background.
  if (add_background == !!add_container_->GetBackground()) {
    return;
  }

  add_container_->SetBackground(
      add_background ? views::CreateThemedRoundedRectBackground(
                           cros_tokens::kCrosSysSystemOnBase, /*radius=*/16.0f)
                     : nullptr);
}

void EditingList::UpdateScrollView(bool scroll_to_bottom) {
  scroll_view_->InvalidateLayout();
  if (scroll_to_bottom) {
    scroll_view_->ScrollByOffset(
        gfx::PointF(0, scroll_content_->GetPreferredSize().height()));
  }

  UpdateWidget();
  UpdateAddContainerBackground(/*add_background=*/!HasScrollOffset());
}

void EditingList::OnScrollViewScrolled() {
  UpdateAddContainerBackground(/*add_background=*/!HasScrollOffset());
}

bool EditingList::HasScrollOffset() {
  return scroll_view_->GetVisibleRect().y() != 0;
}

void EditingList::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_event_pos_ = event.location();
}

void EditingList::OnDragUpdate(const ui::LocatedEvent& event) {
  auto* widget = GetWidget();
  DCHECK(widget);

  controller_->RemoveDeleteEditShortcutWidget();
  auto widget_bounds = widget->GetNativeWindow()->GetBoundsInScreen();
  widget_bounds.Offset(
      /*horizontal=*/(event.location() - start_drag_event_pos_).x(),
      /*vertical=*/0);
  widget->SetBounds(widget_bounds);
}

void EditingList::OnDragEnd(const ui::LocatedEvent& event) {
  UpdateWidget();
}

gfx::Point EditingList::GetWidgetMagneticPositionLocal() {
  auto* widget = GetWidget();
  DCHECK(widget);

  const auto width = GetPreferredSize().width();
  const auto anchor_bounds = controller_->touch_injector()->content_bounds();
  const auto available_bounds = CalculateAvailableBounds(
      controller_->touch_injector()->window()->GetRootWindow());

  // Check if there is space on left and right side outside of the sibling game
  // window.
  bool has_space_on_left =
      anchor_bounds.x() - width - kEditingListSpaceBetweenMainWindow >= 0;
  bool has_space_on_right =
      anchor_bounds.right() + width + kEditingListSpaceBetweenMainWindow <
      available_bounds.width();

  // Check if the attached widget should be inside or outside of the attached
  // sibling game window.
  bool should_be_outside = has_space_on_left || has_space_on_right;

  // Check if the attached widget should be on left or right side of the
  // attached sibling game window.
  auto center = widget->GetNativeWindow()->bounds().CenterPoint();
  auto anchor_center = anchor_bounds.CenterPoint();
  bool should_be_on_left = center.x() < anchor_center.x();
  // Prefer to have the attached widget outside of the sibling game window if
  // there is enough space on left or right. Otherwise, apply the attached
  // widget inside of the sibling game window.
  bool on_left_side =
      (should_be_outside &&
       ((has_space_on_left && should_be_on_left) || !has_space_on_right)) ||
      (!should_be_outside && should_be_on_left);

  // Calculate attached widget origin in root window.
  gfx::Point window_origin = anchor_bounds.origin();
  if (on_left_side) {
    window_origin.SetPoint(
        should_be_outside
            ? anchor_bounds.x() - width - kEditingListSpaceBetweenMainWindow
            : anchor_bounds.x() + kEditingListOffsetInsideMainWindow,
        should_be_outside
            ? window_origin.y()
            : window_origin.y() + kEditingListOffsetInsideMainWindow);
  } else {
    window_origin.SetPoint(
        should_be_outside
            ? anchor_bounds.right() + kEditingListSpaceBetweenMainWindow
            : anchor_bounds.right() - width -
                  kEditingListOffsetInsideMainWindow,
        should_be_outside
            ? window_origin.y()
            : window_origin.y() + kEditingListOffsetInsideMainWindow);
  }

  ClipScrollViewHeight(should_be_outside);

  return window_origin;
}

void EditingList::ClipScrollViewHeight(bool is_outside) {
  int max_height = controller_->touch_injector()->content_bounds().height() -
                   add_container_->GetPreferredSize().height() -
                   2 * kEditingListInsideBorderInsets - kHeaderBottomMargin -
                   kAddRowBottomMargin - kIconButtonSize;
  if (!is_outside) {
    max_height -= kEditingListOffsetInsideMainWindow;
  }

  scroll_view_->ClipHeightTo(/*min_height=*/0, /*max_height=*/max_height);
}

gfx::Size EditingList::CalculatePreferredSize() const {
  return gfx::Size(kMainContainerWidth, GetHeightForWidth(kMainContainerWidth));
}

void EditingList::VisibilityChanged(View* starting_from, bool is_visible) {
  if (is_visible && is_zero_state_) {
    // TODO(b/274690042): Replace it with localized strings.
    controller_->AddNudgeWidget(add_button_, u"Add your first button here");
  }
}

void EditingList::OnThemeChanged() {
  views::View::OnThemeChanged();

  // Set up highlight and focus ring for `add_button_`.
  ash::StyleUtil::SetUpInkDropForButton(
      /*button=*/add_button_, gfx::Insets(), /*highlight_on_hover=*/true,
      /*highlight_on_focus=*/true, /*background_color=*/
      GetColorProvider()->GetColor(cros_tokens::kCrosSysHoverOnProminent));

  // `StyleUtil::SetUpInkDropForButton()` reinstalls the focus ring, so it
  // needs to set the focus ring size after calling
  // `StyleUtil::SetUpInkDropForButton()`.
  auto* focus_ring = views::FocusRing::Get(add_button_);
  focus_ring->SetHaloInset(kHaloInset);
  focus_ring->SetHaloThickness(kHaloThickness);
}

void EditingList::OnActionAdded(Action& action) {
  DCHECK(scroll_content_);
  if (controller_->GetActiveActionsSize() == 1u) {
    // Clear the zero-state.
    controller_->RemoveNudgeWidget(GetWidget());
    UpdateOnZeroState(/*is_zero_state=*/false);
  }
  scroll_content_->AddChildView(
      std::make_unique<ActionViewListItem>(controller_, &action));
  // Scroll the list to bottom when a new action is added.
  UpdateScrollView(/*scroll_to_bottom=*/true);

  UpdateAddButtonState();
}

void EditingList::OnActionRemoved(const Action& action) {
  DCHECK(scroll_content_);
  for (auto* child : scroll_content_->children()) {
    auto* list_item = static_cast<ActionViewListItem*>(child);
    DCHECK(list_item);
    if (list_item->action() == &action) {
      scroll_content_->RemoveChildViewT(list_item);
      UpdateScrollView(/*scroll_to_bottom=*/false);
      break;
    }
  }
  // Set to zero-state if it is empty.
  if (controller_->GetActiveActionsSize() == 0u) {
    UpdateOnZeroState(/*is_zero_state=*/true);
    // TODO(b/274690042): Replace it with localized strings.
    controller_->AddNudgeWidget(add_button_, u"Add your first button here");
  }

  UpdateAddButtonState();
}

void EditingList::OnActionTypeChanged(Action* action, Action* new_action) {
  DCHECK(!is_zero_state_);
  for (size_t i = 0; i < scroll_content_->children().size(); i++) {
    auto* list_item =
        static_cast<ActionViewListItem*>(scroll_content_->children()[i]);
    DCHECK(list_item);
    if (list_item->action() == action) {
      scroll_content_->RemoveChildViewT(list_item);
      scroll_content_->AddChildViewAt(
          std::make_unique<ActionViewListItem>(controller_, new_action), i);
      UpdateScrollView(/*scroll_to_bottom=*/false);
      return;
    }
  }
}

void EditingList::OnActionInputBindingUpdated(const Action& action) {
  DCHECK(scroll_content_);
  for (auto* child : scroll_content_->children()) {
    auto* list_item = static_cast<ActionViewListItem*>(child);
    DCHECK(list_item);
    if (list_item->action() == &action) {
      list_item->OnActionInputBindingUpdated();
      break;
    }
  }
}

void EditingList::OnActionNameUpdated(const Action& action) {
  DCHECK(scroll_content_);
  for (auto* child : scroll_content_->children()) {
    auto* list_item = static_cast<ActionViewListItem*>(child);
    DCHECK(list_item);
    if (list_item->action() == &action) {
      list_item->OnActionNameUpdated();
      break;
    }
  }
}

void EditingList::OnActionNewStateRemoved(const Action& action) {
  DCHECK(scroll_content_);
  for (auto* child : scroll_content_->children()) {
    auto* list_item = static_cast<ActionViewListItem*>(child);
    DCHECK(list_item);
    if (list_item->action() == &action) {
      list_item->RemoveNewState();
      break;
    }
  }
}

BEGIN_METADATA(EditingList, views::View)
END_METADATA

}  // namespace arc::input_overlay
