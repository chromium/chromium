// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/unified/feature_tile.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/vector_icons.h"

namespace arc::input_overlay {

namespace {

// Whole Menu measurements.
constexpr int kMenuWidth = 316;

// Triangle.
constexpr int kTriangleLength = 20;
constexpr int kTriangleHeight = 14;
constexpr int kCornerRadius = 16;
constexpr int kBorderThickness = 2;

// Draws the dialog shape path with round corner. It starts after the corner
// radius on line #0 and draws clockwise.
//  _0>__________
// |             |
// |             |
// |             |
// |              >
// |             |
// |             |
// |_____________|
//
SkPath BackgroundPath(int height) {
  SkPath path;
  auto short_length = kMenuWidth - kTriangleHeight - 2 * kCornerRadius;
  auto short_height = height - 2 * kCornerRadius;
  path.moveTo(kCornerRadius, 0);
  // Top left after corner radius to top right corner radius.
  path.rLineTo(short_length, 0);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, +kCornerRadius, +kCornerRadius);
  // Top right after corner radius to midway point.
  path.rLineTo(0, short_height / 2 - kTriangleLength / 2);
  // Triangle shape.
  path.rLineTo(kTriangleHeight, kTriangleLength / 2);
  path.rLineTo(-kTriangleHeight, kTriangleLength / 2);
  // After midway point to bottom right corner radius.
  path.rLineTo(0, short_height / 2 - kTriangleLength / 2);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -kCornerRadius, +kCornerRadius);
  // Bottom right after corner radius to bottom left corner radius.
  path.rLineTo(-short_length, 0);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -kCornerRadius, -kCornerRadius);
  // Bottom left after corner radius to top left corner radius.
  path.rLineTo(0, -short_height);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, +kCornerRadius, -kCornerRadius);
  // Path finish.
  path.close();
  return path;
}

}  // namespace

// static
ButtonOptionsMenu* ButtonOptionsMenu::Show(DisplayOverlayController* controller,
                                           Action* action) {
  // Ensure there is only one menu at any time.
  if (controller->HasButtonOptionsMenu()) {
    controller->RemoveButtonOptionsMenu();
  }

  auto* parent = controller->GetOverlayWidgetContentsView();
  auto* button_options_menu = parent->AddChildView(
      std::make_unique<ButtonOptionsMenu>(controller, action));
  button_options_menu->Init();
  return button_options_menu;
}

ButtonOptionsMenu::ButtonOptionsMenu(DisplayOverlayController* controller,
                                     Action* action)
    : display_overlay_controller_(controller), action_(action) {}

ButtonOptionsMenu::~ButtonOptionsMenu() = default;

void ButtonOptionsMenu::Init() {
  SetUseDefaultFillLayout(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(16, 16, 16, 16 + kTriangleHeight)));

  AddHeader();
  AddEditTitle();
  AddActionSelection();
  AddActionEdit();
  AddActionNameLabel();

  SizeToPreferredSize();
  CalculatePosition();
}

void ButtonOptionsMenu::AddHeader() {
  // ------------------------------------
  // ||icon|  |"Button options"|  |icon||
  // ------------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/1.0f,
                  views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/2.0f,
                 views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, views::TableLayout::kFixedSize, 0);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 16, 0));

  container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&ButtonOptionsMenu::OnTrashButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDeleteIcon,
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));

  container->AddChildView(ash::bubble_utils::CreateLabel(
      // TODO(b/274690042): Replace placeholder text with localized strings.
      ash::TypographyToken::kCrosTitle1, u"Button options",
      cros_tokens::kCrosSysOnSurface));

  container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&ButtonOptionsMenu::OnDoneButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &kGameControlsDoneIcon,
      // TODO(b/279117180): Replace placeholder names with a11y strings.
      IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
}

void ButtonOptionsMenu::AddEditTitle() {
  // ------------------------------
  // ||"Key assignment"|          |
  // ------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 12, 0));

  container->AddChildView(ash::bubble_utils::CreateLabel(
      // TODO(b/274690042): Replace placeholder text with localized strings.
      ash::TypographyToken::kCrosBody2, u"Key assignment",
      cros_tokens::kCrosSysOnSurface));
}

void ButtonOptionsMenu::AddActionSelection() {
  // ----------------------------------
  // | |feature_tile| |feature_title| |
  // ----------------------------------
  auto* container = AddChildView(std::make_unique<ash::RoundedContainer>(
      ash::RoundedContainer::Behavior::kTopRounded));
  // Create a 1x2 table with a column padding of 8.
  container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStretch,
                  views::LayoutAlignment::kStretch,
                  /*horizontal_resize=*/1.0f,
                  views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/8)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch,
                 /*horizontal_resize=*/1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, views::TableLayout::kFixedSize, 0);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 2, 0));

  auto* tap_button = container->AddChildView(std::make_unique<ash::FeatureTile>(
      base::BindRepeating(&ButtonOptionsMenu::OnTapButtonPressed,
                          base::Unretained(this)),
      /*is_togglable=*/true,
      /*type=*/ash::FeatureTile::TileType::kCompact));
  tap_button->SetID(ash::VIEW_ID_ACCESSIBILITY_FEATURE_TILE);
  tap_button->SetAccessibleName(
      // TODO(b/279117180): Replace placeholder names with a11y strings.
      l10n_util::GetStringUTF16(IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
  // TODO(b/274690042): Replace placeholder text with localized strings.
  tap_button->SetLabel(u"Single button");
  tap_button->SetVectorIcon(vector_icons::kCloseIcon);
  tap_button->SetVisible(true);
  tap_button->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));

  auto* move_button =
      container->AddChildView(std::make_unique<ash::FeatureTile>(
          base::BindRepeating(&ButtonOptionsMenu::OnMoveButtonPressed,
                              base::Unretained(this)),
          /*is_togglable=*/true,
          /*type=*/ash::FeatureTile::TileType::kCompact));
  move_button->SetID(ash::VIEW_ID_ACCESSIBILITY_FEATURE_TILE);
  move_button->SetAccessibleName(
      // TODO(b/279117180): Replace placeholder names with a11y strings.
      l10n_util::GetStringUTF16(IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
  // TODO(b/274690042): Replace placeholder text with localized strings.
  move_button->SetLabel(u"Dpad");
  move_button->SetVectorIcon(kGameControlsDpadKeyboardIcon);
  move_button->SetVisible(true);
  move_button->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
}

void ButtonOptionsMenu::AddActionEdit() {
  // ------------------------------
  // ||"Selected key" |key labels||
  // ||"key"                      |
  // ------------------------------
  auto* container = AddChildView(std::make_unique<ash::RoundedContainer>(
      ash::RoundedContainer::Behavior::kBottomRounded));
  container->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/1.0f,
                  views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, views::TableLayout::kFixedSize, 0);
  container->SetBorderInsets(gfx::Insets::VH(14, 16));
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 8, 0));

  // TODO(b/274690042): Replace placeholder text with localized strings.
  container->AddChildView(CreateNameTag(u"Selected key", u"Key"));
  switch (action_->GetType()) {
    case ActionType::TAP:
      container->AddChildView(CreateActionTapEditForKeyboard(action_));
      break;
    case ActionType::MOVE:
      container->AddChildView(CreateActionMoveEditForKeyboard(action_));
      break;
    default:
      NOTREACHED();
  }
}

void ButtonOptionsMenu::AddActionNameLabel() {
  // ------------------------------
  // ||"Button label"           > |
  // ||"Unassigned"               |
  //  -----------------------------
  auto* container = AddChildView(std::make_unique<ash::RoundedContainer>());
  container->SetUseDefaultFillLayout(true);
  container->SetBorderInsets(gfx::Insets::VH(14, 16));

  auto* action_name_feature_tile =
      container->AddChildView(std::make_unique<ash::FeatureTile>(
          base::BindRepeating(
              &ButtonOptionsMenu::OnButtonLabelAssignmentPressed,
              base::Unretained(this)),
          /*is_togglable=*/false));
  action_name_feature_tile->SetID(ash::VIEW_ID_ACCESSIBILITY_FEATURE_TILE);
  action_name_feature_tile->SetAccessibleName(
      // TODO(b/279117180): Replace placeholder names with a11y strings.
      l10n_util::GetStringUTF16(IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
  // TODO(b/274690042): Replace placeholder text with localized strings.
  action_name_feature_tile->SetLabel(u"Button label");
  action_name_feature_tile->SetSubLabel(u"Unassigned");
  action_name_feature_tile->SetSubLabelVisibility(true);
  action_name_feature_tile->CreateDecorativeDrillInArrow();
  action_name_feature_tile->SetBackground(
      views::CreateSolidBackground(SK_ColorTRANSPARENT));
  action_name_feature_tile->SetVisible(true);
}

void ButtonOptionsMenu::CalculatePosition() {
  auto position = action_->GetUICenterPosition();
  auto x = position.x();
  auto y = position.y();
  auto parent_size =
      display_overlay_controller_->GetOverlayWidgetContentsView()->size();

  // Set the menu at the middle if there is not enough margin on the right
  // or left side.
  if (x + width() > parent_size.width() || x < 0) {
    x = std::max(0, parent_size.width() - width());
  }

  // Set the menu at the bottom if there is not enough margin on the bottom
  // side.
  if (y + height() > parent_size.height()) {
    y = std::max(0, parent_size.height() - height());
  }

  SetPosition(gfx::Point(x, y));
}

void ButtonOptionsMenu::OnTrashButtonPressed() {
  // TODO(b/270969760): Implement close menu functionality.
  display_overlay_controller_->RemoveButtonOptionsMenu();
}

void ButtonOptionsMenu::OnDoneButtonPressed() {
  // TODO(b/270969760): Implement save menu functionality.
  display_overlay_controller_->RemoveButtonOptionsMenu();
}

void ButtonOptionsMenu::OnTapButtonPressed() {
  // TODO(b/270969760): Implement tap button functionality.
}

void ButtonOptionsMenu::OnMoveButtonPressed() {
  // TODO(b/270969760): Implement move button functionality.
}

void ButtonOptionsMenu::OnButtonLabelAssignmentPressed() {
  // TODO(b/270969760): Implement key binding change functionality.
}

void ButtonOptionsMenu::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  // Draw the shape.
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  ui::ColorProvider* color_provider = GetColorProvider();
  flags.setColor(color_provider->GetColor(cros_tokens::kCrosSysBaseElevated));
  int height = GetHeightForWidth(kMenuWidth);
  canvas->DrawPath(BackgroundPath(height), flags);
  // Draw the border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  // TODO(b/270969760): Change to "sys.BorderHighlight1" when added.
  flags.setColor(color_provider->GetColor(cros_tokens::kCrosSysSystemBorder1));
  flags.setStrokeWidth(kBorderThickness);
  canvas->DrawPath(BackgroundPath(height), flags);
}

gfx::Size ButtonOptionsMenu::CalculatePreferredSize() const {
  // TODO(b/270969760): Dynamically calculate height based on action selection.
  return gfx::Size(kMenuWidth, GetHeightForWidth(kMenuWidth));
}

}  // namespace arc::input_overlay
