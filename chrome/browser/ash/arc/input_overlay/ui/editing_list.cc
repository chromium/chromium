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
#include "chrome/grit/component_extension_resources.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"

namespace arc::input_overlay {

namespace {

constexpr int kMainContainerWidth = 296;

}  // namespace

EditingList::EditingList(DisplayOverlayController* controller)
    : TouchInjectorObserver(), controller_(controller) {
  controller_->AddTouchInjectorObserver(this);
  Init();
}

EditingList::~EditingList() {
  controller_->RemoveTouchInjectorObserver(this);
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
      cros_tokens::kCrosSysSystemBaseElevated));
  main_container->SetBorderInsets(gfx::Insets::VH(16, 16));
  main_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  AddHeader(main_container);

  scroll_content_ =
      main_container->AddChildView(std::make_unique<views::View>());
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
  return controller_->GetActiveActionsSize() != 0;
}

void EditingList::AddHeader(views::View* container) {
  auto* header_container =
      container->AddChildView(std::make_unique<views::View>());
  header_container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter, 1.0f,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                 1.0f, views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize);
  header_container->SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(0, 0, 16, 0));
  header_container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&EditingList::OnAddButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsAddIcon,
      // TODO(b/279117180): Update a11y string.
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
  header_container->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosTitle1,
      // TODO(b/274690042): Replace it with localized strings.
      u"Editing", cros_tokens::kCrosSysOnSurface));
  header_container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&EditingList::OnDoneButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDoneIcon,
      // TODO(b/279117180): Update a11y string.
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
}

void EditingList::AddZeroStateContent() {
  is_zero_state_ = true;

  DCHECK(scroll_content_);
  auto* content_container =
      scroll_content_->AddChildView(std::make_unique<ash::RoundedContainer>());
  content_container->SetBackground(
      views::CreateThemedSolidBackground(cros_tokens::kCrosSysSystemOnBase));
  content_container->SetBorderInsets(gfx::Insets::VH(48, 32));
  content_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  auto* zero_banner =
      content_container->AddChildView(std::make_unique<views::ImageView>());
  zero_banner->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
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

gfx::Size EditingList::CalculatePreferredSize() const {
  return gfx::Size(kMainContainerWidth, GetHeightForWidth(kMainContainerWidth));
}

void EditingList::OnActionAdded(Action& action) {
  DCHECK(scroll_content_);
  if (controller_->GetActiveActionsSize() == 1u) {
    // Clear the zero-state.
    scroll_content_->RemoveAllChildViews();
    controller_->TurnFlag(ash::ArcGameControlsFlag::kEmpty, /*turn_on=*/false);
  }
  scroll_content_->AddChildView(
      std::make_unique<ActionViewListItem>(controller_, &action));

  controller_->UpdateEditingListWidgetBounds();
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
    controller_->TurnFlag(ash::ArcGameControlsFlag::kEmpty, /*turn_on=*/true);
  }

  controller_->UpdateEditingListWidgetBounds();
}

void EditingList::OnActionTypeChanged(Action* action, Action* new_action) {
  OnActionRemoved(*action);
  OnActionAdded(*new_action);
  controller_->UpdateEditingListWidgetBounds();
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

void EditingList::OnDragStart(const ui::LocatedEvent& event) {
  start_drag_event_pos_ = event.location();
  start_drag_pos_ = origin();
  window_bounds_ = controller_->GetEditingListWidgetBoundsInRootWindow();
}

void EditingList::OnDragUpdate(const ui::LocatedEvent& event) {
  auto target_position = origin() + (event.location() - start_drag_event_pos_);
  ClampPosition(target_position);
  SetPosition(target_position);
}

void EditingList::OnDragEnd(const ui::LocatedEvent& event) {
  auto reposition_delta = origin() - start_drag_pos_;
  controller_->UpdateEditingListWidgetPosition(reposition_delta);
  SetPosition(gfx::Point(0, 0));
}

void EditingList::ClampPosition(gfx::Point& position) {
  position.set_x(std::clamp(position.x(), window_bounds_.x(),
                            window_bounds_.right() - width()));
  position.set_y(std::clamp(position.y(), window_bounds_.y(),
                            window_bounds_.bottom() - height()));
}

}  // namespace arc::input_overlay
