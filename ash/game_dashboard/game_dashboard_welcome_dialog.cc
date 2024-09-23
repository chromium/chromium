// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_welcome_dialog.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/game_dashboard/game_dashboard_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// Corner radius of the welcome dialog.
constexpr float kDialogCornerRadius = 24.0f;
// Fixed width of the welcome dialog.
constexpr int kDialogWidth = 360;
// Radius of the icon and its background displayed in the dialog.
constexpr float kIconBackgroundRadius = 40.0f;
// The height and width of the dialog's icon.
constexpr int kIconSize = 20;
// Additional padding for the top, left, and right title container border.
constexpr int kPrimaryContainerBorder = 12;
// Border padding surrounding the inside of the entire welcome dialog.
constexpr int kPrimaryLayoutInsideBorder = 8;
// Padding between the `primary_container` and `shortcut_hint` rows.
constexpr int kRowPadding = 20;
// Padding between the `title_container` and `icon_container`.
constexpr int kTitleContainerPadding = 20;
// Width of the container containing the text title and sub-label.
constexpr int kTitleTextMaxWidth =
    kDialogWidth - kIconBackgroundRadius - kTitleContainerPadding -
    /*left and right dialog insets*/ 2 * kPrimaryLayoutInsideBorder -
    /*additional `primary_container` left and right padding*/
    2 * kPrimaryContainerBorder;
// Maximum duration that the dialog should be displayed.
constexpr base::TimeDelta kDialogDuration = base::Seconds(4);

}  // namespace

GameDashboardWelcomeDialog::GameDashboardWelcomeDialog() {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetIgnoreDefaultMainAxisMargins(true);
  SetDefault(views::kMarginsKey, gfx::Insets::TLBR(kRowPadding, 0, 0, 0));
  SetInteriorMargin(
      gfx::Insets::VH(kPrimaryLayoutInsideBorder, kPrimaryLayoutInsideBorder));
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque, kDialogCornerRadius));
  SetBorder(views::CreateThemedRoundedRectBorder(
      game_dashboard::kWelcomeDialogBorderThickness, kDialogCornerRadius,
      ui::ColorIds::kColorHighlightBorderHighlight1));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(kDialogCornerRadius);

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_GAME_DASHBOARD_WELCOME_DIALOG_A11Y_LABEL));

  AddTitleAndIconRow();
  AddShortcutInfoRow();
}

GameDashboardWelcomeDialog::~GameDashboardWelcomeDialog() = default;

void GameDashboardWelcomeDialog::StartTimer(base::OnceClosure on_complete) {
  DCHECK(on_complete) << "OnceClosure must be passed to determine what to do "
                         "when the timer completes.";
  timer_.Start(FROM_HERE, kDialogDuration, std::move(on_complete));
}

void GameDashboardWelcomeDialog::AnnounceForAccessibility() {
  GetViewAccessibility().AnnounceAlert(l10n_util::GetStringFUTF16(
      IDS_ASH_GAME_DASHBOARD_WELCOME_DIALOG_A11Y_ANNOUNCEMENT,
      l10n_util::GetStringUTF16(
          Shell::Get()->keyboard_capability()->HasLauncherButtonOnAnyKeyboard()
              ? IDS_ASH_SHORTCUT_MODIFIER_LAUNCHER
              : IDS_ASH_SHORTCUT_MODIFIER_SEARCH)));
}

// Creates a primary container that holds separate sub-containers for the text
// and icon.
// Note: When using `views::FlexLayoutView` it's common to wrap objects in
// additional containers that need a separate alignment than the rest of the
// elements. This creates the following:
//
// +----------------------------------------------------+
// |                 primary_container                  |
// |  +--------------------------+-------------------+  |
// |  |     title_container      |   icon_container  |  |
// |  |  +--------------------+  |        +--------+ |  |
// |  |  |       title        |  |        |        | |  |
// |  |  +--------------------+  |        |  icon  | |  |
// |  |  |     sub_label      |  |        |        | |  |
// |  |  +--------------------+  |        +--------+ |  |
// |  +--------------------------+-------------------+  |
// +----------------------------------------------------+
void GameDashboardWelcomeDialog::AddTitleAndIconRow() {
  auto* primary_container =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  primary_container->SetIgnoreDefaultMainAxisMargins(true);
  primary_container->SetDefault(views::kMarginsKey, gfx::Insets::VH(0, 0));
  primary_container->SetInteriorMargin(
      gfx::Insets::TLBR(kPrimaryContainerBorder, kPrimaryContainerBorder, 0,
                        kPrimaryContainerBorder));
  primary_container->SetOrientation(views::LayoutOrientation::kHorizontal);

  // Create title container as a child of the primary container.
  auto* title_container = primary_container->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  title_container->SetOrientation(views::LayoutOrientation::kVertical);
  title_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  title_container->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  title_container->SetInteriorMargin(
      gfx::Insets::TLBR(0, 0, 0, kTitleContainerPadding));

  // Add title label to the title container.
  auto* title = title_container->AddChildView(bubble_utils::CreateLabel(
      TypographyToken::kCrosButton1,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_TITLE),
      cros_tokens::kCrosSysOnSurface));
  title->SetMultiLine(true);
  title->SizeToFit(kTitleTextMaxWidth);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Add sub-label to the title container.
  auto* sub_label = title_container->AddChildView(bubble_utils::CreateLabel(
      TypographyToken::kCrosAnnotation2,
      l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_WELCOME_DIALOG_SUB_LABEL),
      cros_tokens::kCrosSysOnSurfaceVariant));
  // TODO(b/316138331): Investigate why multi-line support isn't working
  // properly.
  sub_label->SetMultiLine(true);
  sub_label->SizeToFit(kTitleTextMaxWidth);
  sub_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Create icon container as a child of the primary container.
  auto* icon_container = primary_container->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  icon_container->SetCrossAxisAlignment(views::LayoutAlignment::kEnd);

  // Add icon to the icon container.
  auto* icon = icon_container->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          chromeos::kGameDashboardGamepadIcon, cros_tokens::kCrosSysOnPrimary,
          kIconSize)));
  icon->SetPreferredSize(
      gfx::Size(kIconBackgroundRadius, kIconBackgroundRadius));
  icon->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysPrimary, kIconBackgroundRadius));
}

// Creates a stylized label that holds the hint indicating how open the Game
// Dashboard shortcut. This creates the following:
//
// +----------------------------------------------------+
// | |"Press" | |Launcher icon| |"+ g at anytime"|      |
// +----------------------------------------------------+
void GameDashboardWelcomeDialog::AddShortcutInfoRow() {
  // Styled label to include the inline icon.
  auto* styled_label = AddChildView(std::make_unique<views::StyledLabel>());
  size_t inline_icon_offset;
  styled_label->SetText(
      l10n_util::GetStringFUTF16(IDS_ASH_GAME_DASHBOARD_WELCOME_DIALOG_SHORTCUT,
                                 u"", &inline_icon_offset));
  styled_label->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, /*radius=*/16.0f));
  styled_label->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(14, 18)));

  const bool has_launcher_keyboard_button =
      Shell::Get()->keyboard_capability()->HasLauncherButtonOnAnyKeyboard();
  styled_label->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_ASH_GAME_DASHBOARD_WELCOME_DIALOG_SHORTCUT,
      l10n_util::GetStringUTF16(has_launcher_keyboard_button
                                    ? IDS_ASH_SHORTCUT_MODIFIER_LAUNCHER
                                    : IDS_ASH_SHORTCUT_MODIFIER_SEARCH)));

  // Text style.
  views::StyledLabel::RangeStyleInfo text_style;
  text_style.custom_font = TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosButton2);
  text_style.override_color_id = cros_tokens::kCrosSysPrimary;
  styled_label->AddStyleRange(gfx::Range(0u, inline_icon_offset), text_style);
  styled_label->AddStyleRange(
      gfx::Range(inline_icon_offset + 1u, styled_label->GetText().size()),
      std::move(text_style));

  // Inline icon.
  auto icon = std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      has_launcher_keyboard_button ? kGdLauncherIcon : kGdSearchIcon,
      cros_tokens::kCrosSysPrimary, /*icon_size=*/16));
  icon->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(0, 1, 0, 4)));
  views::StyledLabel::RangeStyleInfo inline_icon_style;
  inline_icon_style.custom_view = icon.get();
  styled_label->AddStyleRange(
      gfx::Range(inline_icon_offset, inline_icon_offset + 1u),
      std::move(inline_icon_style));

  // Add the icon into the styled label.
  styled_label->AddCustomView(std::move(icon));
}

BEGIN_METADATA(GameDashboardWelcomeDialog)
END_METADATA

}  // namespace ash
