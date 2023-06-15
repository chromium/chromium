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
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
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

// Spacing.
constexpr int kMenuActionGap = 8;

// Draws the dialog shape path with round corner. It starts after the corner
// radius on line #0 and draws clockwise.
//
// draw_triangle_on_left draws the triangle wedge on the left side of the box
// instead of the right if set to true.
//
// action_offset draws the triangle wedge higher or lower if the position of
// the action is too close to the top or bottom of the window. An offset of
// zero draws the triangle wedge at the vertical center of the box.
//  _0>__________
// |             |
// |             |
// |             |
// |              >
// |             |
// |             |
// |_____________|
//
SkPath BackgroundPath(int height,
                      bool draw_triangle_on_left,
                      int action_offset) {
  SkPath path;
  int short_length = kMenuWidth - kTriangleHeight - 2 * kCornerRadius;
  int short_height = height - 2 * kCornerRadius;
  // If the offset is greater than the limit or less than the negative
  // limit, set it respectively.
  const int limit = short_height / 2 - kTriangleLength / 2;
  if (action_offset > limit) {
    action_offset = limit;
  } else if (action_offset < -limit) {
    action_offset = -limit;
  }
  if (draw_triangle_on_left) {
    path.moveTo(kCornerRadius + kTriangleHeight, 0);
  } else {
    path.moveTo(kCornerRadius, 0);
  }
  // Top left after corner radius to top right corner radius.
  path.rLineTo(short_length, 0);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, +kCornerRadius, +kCornerRadius);
  if (draw_triangle_on_left) {
    // Top right after corner radius to bottom right corner radius.
    path.rLineTo(0, short_height);
  } else {
    // Top right after corner radius to midway point.
    path.rLineTo(0, limit + action_offset);
    // Triangle shape.
    path.rLineTo(kTriangleHeight, kTriangleLength / 2);
    path.rLineTo(-kTriangleHeight, kTriangleLength / 2);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, limit - action_offset);
  }
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -kCornerRadius, +kCornerRadius);
  // Bottom right after corner radius to bottom left corner radius.
  path.rLineTo(-short_length, 0);
  path.rArcTo(kCornerRadius, kCornerRadius, 0, SkPath::kSmall_ArcSize,
              SkPathDirection::kCW, -kCornerRadius, -kCornerRadius);
  if (draw_triangle_on_left) {
    // bottom left after corner radius to midway point.
    path.rLineTo(0, -limit + action_offset);
    // Triangle shape.
    path.rLineTo(-kTriangleHeight, -kTriangleLength / 2);
    path.rLineTo(kTriangleHeight, -kTriangleLength / 2);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, -limit - action_offset);
  } else {
    // Bottom left after corner radius to top left corner radius.
    path.rLineTo(0, -short_height);
  }
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
  auto* parent = controller->GetOverlayWidgetContentsView();
  auto* button_options_menu = parent->AddChildView(
      std::make_unique<ButtonOptionsMenu>(controller, action));
  button_options_menu->Init();
  return button_options_menu;
}

ButtonOptionsMenu::ButtonOptionsMenu(DisplayOverlayController* controller,
                                     Action* action)
    : TouchInjectorObserver(), controller_(controller), action_(action) {
  controller_->AddTouchInjectorObserver(this);
}

ButtonOptionsMenu::~ButtonOptionsMenu() {
  controller_->RemoveTouchInjectorObserver(this);
}

void ButtonOptionsMenu::Init() {
  SetUseDefaultFillLayout(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetBorder(views::CreateEmptyBorder(
      action_->on_left_or_middle_side()
          ? gfx::Insets::TLBR(16, 16 + kTriangleHeight, 16, 16)
          : gfx::Insets::TLBR(16, 16, 16, 16 + kTriangleHeight)));

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

  auto labels_view = EditLabels::CreateEditLabels(controller_, action_);
  // TODO(b/274690042): Replace placeholder text with localized strings.
  labels_name_tag_ = container->AddChildView(NameTag::CreateNameTag(
      u"Selected key", labels_view->GetTextForNameTag()));
  labels_view_ = container->AddChildView(std::move(labels_view));
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
  auto* action_view = action_->action_view();
  int x = action_view->x();
  int y = action_->GetUICenterPosition().y();
  auto parent_size = controller_->GetOverlayWidgetContentsView()->size();

  if (action_->on_left_or_middle_side()) {
    x += action_view->width() + kMenuActionGap;
  } else {
    x -= width() + kMenuActionGap;
  }

  y -= height() / 2;
  // The range of values for the y-position of the menu are [0, parent_height -
  // height]. If the calculated y-position is beyond this range, adjust it based
  // on whether the y-position is over or under the range by setting it to the
  // maximum or minimum value respectively.
  if (y > parent_size.height() - height()) {
    y = std::max(0, parent_size.height() - height());
  } else if (y < 0) {
    y = 0;
  }

  SetPosition(gfx::Point(x, y));
}

void ButtonOptionsMenu::OnTrashButtonPressed() {
  controller_->RemoveAction(action_);
}

void ButtonOptionsMenu::OnDoneButtonPressed() {
  // TODO(b/270969760): Implement save menu functionality.
  controller_->RemoveButtonOptionsMenu();
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
  bool draw_triangle_on_left = action_->on_left_or_middle_side();
  int action_offset = CalculateActionOffset(height);
  canvas->DrawPath(BackgroundPath(height, draw_triangle_on_left, action_offset),
                   flags);
  // Draw the border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  // TODO(b/270969760): Change to "sys.BorderHighlight1" when added.
  flags.setColor(color_provider->GetColor(cros_tokens::kCrosSysSystemBorder1));
  flags.setStrokeWidth(kBorderThickness);
  canvas->DrawPath(BackgroundPath(height, draw_triangle_on_left, action_offset),
                   flags);
}

gfx::Size ButtonOptionsMenu::CalculatePreferredSize() const {
  // TODO(b/270969760): Dynamically calculate height based on action selection.
  return gfx::Size(kMenuWidth, GetHeightForWidth(kMenuWidth));
}

void ButtonOptionsMenu::OnActionRemoved(const Action& action) {
  DCHECK_EQ(action_, &action);
  controller_->RemoveButtonOptionsMenu();
}

void ButtonOptionsMenu::OnActionTypeChanged(const Action& action,
                                            const Action& new_action) {
  NOTIMPLEMENTED();
}

void ButtonOptionsMenu::OnActionUpdated(const Action& action) {
  if (action_ == &action) {
    labels_view_->OnActionUpdated();
    labels_name_tag_->SetSubtitle(labels_view_->GetTextForNameTag());
  }
}

int ButtonOptionsMenu::CalculateActionOffset(int height) {
  int action_center_y = action_->GetUICenterPosition().y();
  // If the position of the action is too close to the top, return the
  // negative difference between the action position and half the height.
  if (action_center_y < height / 2) {
    return action_center_y - height / 2;
  }
  // If the position of the action is too close to the bottom, return
  // the positive difference between the action position and half the
  // height.
  if (action_center_y > parent()->height() - height / 2) {
    return action_center_y - (parent()->height() - height / 2);
  }
  // Otherwise, return an offset of zero.
  return 0;
}

}  // namespace arc::input_overlay
