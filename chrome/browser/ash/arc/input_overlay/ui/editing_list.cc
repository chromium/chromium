// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/grit/component_extension_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace arc::input_overlay {

namespace {

constexpr int kMainContainerWidth = 296;

constexpr int kInsideBorderInsets = 16;
constexpr int kHeaderBottomMargin = 16;
// This is associated to the size of `ash::IconButton::Type::kMedium`.
constexpr int kIconButtonSize = 32;

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
  SetUseDefaultFillLayout(true);

  // Main container.
  auto* main_container =
      AddChildView(std::make_unique<ash::RoundedContainer>());
  main_container->SetBackground(views::CreateThemedSolidBackground(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque));
  main_container->SetBorderInsets(
      gfx::Insets::VH(kInsideBorderInsets, kInsideBorderInsets));
  main_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  AddHeader(main_container);

  scroll_view_ =
      main_container->AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetBackgroundColor(absl::nullopt);
  scroll_content_ = scroll_view_->SetContents(std::make_unique<views::View>());
  scroll_content_
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          /*inside_border_insets=*/gfx::Insets(),
          /*between_child_spacing=*/8))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  // Add contents.
  if (HasControls()) {
    AddControlListContent();
  } else {
    AddZeroStateContent();
  }

  SizeToPreferredSize();
}

bool EditingList::HasControls() const {
  DCHECK(controller_);
  return controller_->GetActiveActionsSize() != 0u;
}

void EditingList::AddHeader(views::View* container) {
  auto* header_container =
      container->AddChildView(std::make_unique<views::View>());
  header_container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kStart,
                  /*v_align=*/views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/views::TableLayout::kFixedSize,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kStretch,
                 /*v_align=*/views::LayoutAlignment::kStretch,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kEnd,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/views::TableLayout::kFixedSize,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, views::TableLayout::kFixedSize);
  header_container->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, 0, kHeaderBottomMargin, 0));
  header_container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&EditingList::OnDoneButtonPressed,
                          base::Unretained(this)),
      // TODO(b/296126993): Add the UX provided back arrow icon.
      ash::IconButton::Type::kMedium, &kBackArrowTouchIcon,
      // TODO(b/279117180): Update a11y string.
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
  editing_header_label_ =
      header_container->AddChildView(ash::bubble_utils::CreateLabel(
          ash::TypographyToken::kCrosTitle1,
          // TODO(b/274690042): Replace it with localized strings.
          u"Editing", cros_tokens::kCrosSysOnSurface));
  add_button_ =
      header_container->AddChildView(std::make_unique<ash::IconButton>(
          base::BindRepeating(&EditingList::OnAddButtonPressed,
                              base::Unretained(this)),
          ash::IconButton::Type::kMedium, &kGameControlsAddIcon,
          // TODO(b/279117180): Update a11y string.
          IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
  UpdateAddButtonState();
}

void EditingList::AddZeroStateContent() {
  is_zero_state_ = true;

  DCHECK(scroll_content_);
  auto* content_container =
      scroll_content_->AddChildView(std::make_unique<views::View>());
  content_container->SetProperty(views::kMarginsKey, gfx::Insets::VH(48, 32));
  content_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  auto* zero_banner =
      content_container->AddChildView(std::make_unique<views::ImageView>());
  zero_banner->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          // TODO(b/301446165): Replace placeholder colors.
          IDS_ARC_INPUT_OVERLAY_ZERO_STATE_ILLUSTRATION_JSON));
  zero_banner->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 32, 0));
  content_container->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosBody2,
      // TODO(b/274690042): Replace it with localized strings.
      u"Your button will show up here.", cros_tokens::kCrosSysSecondary));
}

void EditingList::AddControlListContent() {
  is_zero_state_ = false;

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

void EditingList::OnAddButtonPressed() {
  controller_->AddNewAction();
}

void EditingList::OnDoneButtonPressed() {
  // TODO(b/270969479): Implement the function for the button.
  DCHECK(controller_);
  controller_->OnCustomizeSave();
}

void EditingList::UpdateAddButtonState() {
  add_button_->SetEnabled(controller_->GetActiveActionsSize() <
                          kMaxActionCount);
}

void EditingList::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_event_pos_ = event.location();
}

void EditingList::OnDragUpdate(const ui::LocatedEvent& event) {
  auto* widget = GetWidget();
  DCHECK(widget);

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
                   2 * kInsideBorderInsets - kHeaderBottomMargin -
                   kIconButtonSize;
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

void EditingList::OnActionAdded(Action& action) {
  DCHECK(scroll_content_);
  if (controller_->GetActiveActionsSize() == 1u) {
    // Clear the zero-state.
    scroll_content_->RemoveAllChildViews();
    controller_->RemoveNudgeWidget(GetWidget());
    is_zero_state_ = false;
  }
  scroll_content_->AddChildView(
      std::make_unique<ActionViewListItem>(controller_, &action));
  scroll_view_->InvalidateLayout();
  // Scroll the list to bottom when a new action is added.
  scroll_view_->ScrollByOffset(
      gfx::PointF(0, scroll_content_->GetPreferredSize().height()));

  UpdateAddButtonState();
  UpdateWidget();
}

void EditingList::OnActionRemoved(const Action& action) {
  DCHECK(scroll_content_);
  for (auto* child : scroll_content_->children()) {
    auto* list_item = static_cast<ActionViewListItem*>(child);
    DCHECK(list_item);
    if (list_item->action() == &action) {
      scroll_content_->RemoveChildViewT(list_item);
      break;
    }
  }
  // Set to zero-state if it is empty.
  if (controller_->GetActiveActionsSize() == 0u) {
    AddZeroStateContent();
    // TODO(b/274690042): Replace it with localized strings.
    controller_->AddNudgeWidget(add_button_, u"Add your first button here");
  } else {
    scroll_view_->InvalidateLayout();
  }

  UpdateAddButtonState();
  UpdateWidget();
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
      scroll_view_->InvalidateLayout();
      break;
    }
  }

  UpdateWidget();
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

}  // namespace arc::input_overlay
