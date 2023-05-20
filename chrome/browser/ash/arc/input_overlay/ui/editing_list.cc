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

// static
EditingList* EditingList::Show(DisplayOverlayController* controller) {
  auto* parent = controller->GetOverlayWidgetContentsView();
  auto* editing_list =
      parent->AddChildView(std::make_unique<EditingList>(controller));
  editing_list->Init();
  editing_list->SetPosition(gfx::Point(24, 24));
  return editing_list;
}

EditingList::EditingList(DisplayOverlayController* controller)
    : controller_(controller) {}
EditingList::~EditingList() = default;

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

  // Add contents.
  if (HasControls()) {
    AddControlListContent(main_container);
  } else {
    AddZeroStateContent(main_container);
  }

  SizeToPreferredSize();
  InvalidateLayout();
}

bool EditingList::HasControls() const {
  DCHECK(controller_);
  return controller_->GetInputMappingListSize() != 0;
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
      ash::IconButton::Type::kMedium, &kAddIcon,
      // TODO(b/279117180): Update a11y string.
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
  header_container->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosTitle1,
      // TODO(b/274690042): Replace it with localized strings.
      u"Editing", cros_tokens::kCrosSysOnSurface));
  header_container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&EditingList::OnDoneButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &ash::kCheckIcon,
      // TODO(b/279117180): Update a11y string.
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
}

void EditingList::AddZeroStateContent(views::View* container) {
  auto* content_container =
      container->AddChildView(std::make_unique<ash::RoundedContainer>());
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
          // TODO(b/270969479): Replace the image once the lottie json is
          // ready.
          IDS_ARC_INPUT_OVERLAY_ONBOARDING_ILLUSTRATION_DARK_JSON));
  // TODO(b/270969479): The size will be removed once the right lottie json is
  // added.
  zero_banner->SetImageSize(gfx::Size(92, 92));
  zero_banner->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 32, 0));
  content_container->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosBody2,
      // TODO(b/274690042): Replace it with localized strings.
      u"Your button will show up here.", cros_tokens::kCrosSysSecondary));
}

void EditingList::AddControlListContent(views::View* container) {
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
  // TODO(b/270969479): Wrap |scroll_content| in a scroll view.
  auto* scroll_content =
      container->AddChildView(std::make_unique<views::View>());
  scroll_content
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          /*inside_border_insets=*/gfx::Insets(),
          /*between_child_spacing=*/8))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  DCHECK(controller_);
  for (const auto& action : controller_->touch_injector()->actions()) {
    scroll_content->AddChildView(
        std::make_unique<ActionViewListItem>(controller_, action.get()));
  }
}

void EditingList::OnAddButtonPressed() {
  // TODO(b/270969479): Implement the function for the button.
  NOTIMPLEMENTED();
}

void EditingList::OnDoneButtonPressed() {
  // TODO(b/270969479): Implement the function for the button.
  DCHECK(controller_);
  controller_->OnCustomizeSave();
}

gfx::Size EditingList::CalculatePreferredSize() const {
  return gfx::Size(kMainContainerWidth, GetHeightForWidth(kMainContainerWidth));
}

}  // namespace arc::input_overlay
