// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/sub_feature_opt_in_view.h"

#include <string>

#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Appearance.
constexpr int kButtonSpacingDip = 8;
constexpr int kBorderThicknessDip = 1;
constexpr int kBorderCornerRadiusDip = 8;
constexpr auto kTextLabelBorderInsets = gfx::Insets::TLBR(12, 16, 12, 16);
constexpr auto kButtonContainerBorderInsets = gfx::Insets::TLBR(0, 0, 12, 16);
constexpr int kTextLabelLineHeightDip = 20;

// Typography.
constexpr int kLabelTextFontSizeDip = 14;

int GetDescriptionStringId(
    PermissionsOnboardingSetUpMode permission_setup_mode) {
  switch (permission_setup_mode) {
    case PermissionsOnboardingSetUpMode::kCameraRoll:
      return IDS_ASH_PHONE_HUB_CAMERA_ROLL_OPT_IN_DESCRIPTION;
    case PermissionsOnboardingSetUpMode::kMessagingApps:
      return IDS_ASH_PHONE_HUB_APPS_OPT_IN_DESCRIPTION;
    case PermissionsOnboardingSetUpMode::kNotificationAndCameraRoll:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_AND_CAMERA_ROLL_OPT_IN_DESCRIPTION;
    case PermissionsOnboardingSetUpMode::kNotification:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DESCRIPTION;
    case PermissionsOnboardingSetUpMode::kNotificationAndMessagingApps:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_AND_APPS_OPT_IN_DESCRIPTION;
    case PermissionsOnboardingSetUpMode::kMessagingAppsAndCameraRoll:
      return IDS_ASH_PHONE_HUB_CAMERA_ROLL_AND_APPS_OPT_IN_DESCRIPTION;
    case PermissionsOnboardingSetUpMode::kAllPermissions:
      return IDS_ASH_PHONE_HUB_ALL_FEATURES_OPT_IN_DESCRIPTION;
    case PermissionsOnboardingSetUpMode::kNone:
    default:
      // Just return the default strings since the MultideviceFeatureOptInView
      // will be invisible.
      return IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DESCRIPTION;
  }
}

int GetSetUpButtonAccessibleNameStringId(
    PermissionsOnboardingSetUpMode permission_setup_mode) {
  switch (permission_setup_mode) {
    case PermissionsOnboardingSetUpMode::kCameraRoll:
      return IDS_ASH_PHONE_HUB_CAMERA_ROLL_OPT_IN_SET_UP_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kMessagingApps:
      return IDS_ASH_PHONE_HUB_APPS_OPT_IN_SET_UP_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kNotificationAndCameraRoll:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_AND_CAMERA_ROLL_OPT_IN_SET_UP_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kNotification:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_SET_UP_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kNotificationAndMessagingApps:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_AND_APPS_OPT_IN_SET_UP_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kMessagingAppsAndCameraRoll:
      return IDS_ASH_PHONE_HUB_CAMERA_ROLL_AND_APPS_OPT_IN_SET_UP_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kAllPermissions:
      return IDS_ASH_PHONE_HUB_ALL_FEATURES_OPT_IN_SET_UP_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kNone:
    default:
      // Just return the default strings since the MultideviceFeatureOptInView
      // will be invisible.
      return IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_SET_UP_BUTTON_ACCESSIBLE_NAME;
  }
}

int GetDismissButtonAccessibleNameStringId(
    PermissionsOnboardingSetUpMode permission_setup_mode) {
  switch (permission_setup_mode) {
    case PermissionsOnboardingSetUpMode::kCameraRoll:
      return IDS_ASH_PHONE_HUB_CAMERA_ROLL_OPT_IN_DISMISS_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kMessagingApps:
      return IDS_ASH_PHONE_HUB_APPS_OPT_IN_DISMISS_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kNotificationAndCameraRoll:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_AND_CAMERA_ROLL_OPT_IN_DISMISS_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kNotification:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DISMISS_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kNotificationAndMessagingApps:
      return IDS_ASH_PHONE_HUB_NOTIFICATION_AND_APPS_OPT_IN_DISMISS_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kMessagingAppsAndCameraRoll:
      return IDS_ASH_PHONE_HUB_CAMERA_ROLL_AND_APPS_OPT_IN_DISMISS_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kAllPermissions:
      return IDS_ASH_PHONE_HUB_ALL_FEATURES_OPT_IN_DISMISS_BUTTON_ACCESSIBLE_NAME;
    case PermissionsOnboardingSetUpMode::kNone:
    default:
      // Just return the default strings since the MultideviceFeatureOptInView
      // will be invisible.
      return IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_DISMISS_BUTTON_ACCESSIBLE_NAME;
  }
}

}  // namespace

SubFeatureOptInView::SubFeatureOptInView(
    PhoneHubViewID view_id,
    PermissionsOnboardingSetUpMode setup_mode)
    : view_id_(view_id), setup_mode_(setup_mode) {
  SetID(view_id_);
  InitLayout();
}

SubFeatureOptInView::~SubFeatureOptInView() = default;

void SubFeatureOptInView::SetSetUpMode(
    PermissionsOnboardingSetUpMode setup_mode) {
  setup_mode_ = setup_mode;
  SetStringIds();
  UpdateLabels();
}

void SubFeatureOptInView::SetStringIds() {
  description_string_id_ = GetDescriptionStringId(setup_mode_);
  set_up_button_accessible_name_string_id_ =
      GetSetUpButtonAccessibleNameStringId(setup_mode_);
  dismiss_button_accessible_name_string_id_ =
      GetDismissButtonAccessibleNameStringId(setup_mode_);
}

void SubFeatureOptInView::UpdateLabels() {
  text_label_->SetText(l10n_util::GetStringUTF16(description_string_id_));
  set_up_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(set_up_button_accessible_name_string_id_));
  dismiss_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(dismiss_button_accessible_name_string_id_));
}

void SubFeatureOptInView::InitLayout() {
  // TODO(b/322067753): Replace usage of |AshColorProvider| with |cros_tokens|.
  const SkColor border_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
  SetBorder(views::CreateRoundedRectBorder(
      kBorderThicknessDip, kBorderCornerRadiusDip, border_color));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);

  SetStringIds();

  // Set up layout row for the text label.
  text_label_ = AddChildView(std::make_unique<views::Label>());
  text_label_->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kCenter);
  text_label_->SetProperty(views::kMarginsKey, kTextLabelBorderInsets);
  text_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded, true));
  auto text_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  text_label_->SetEnabledColor(text_color);
  text_label_->SetAutoColorReadabilityEnabled(false);
  auto default_font = text_label_->font_list();
  text_label_->SetFontList(default_font
                               .DeriveWithSizeDelta(kLabelTextFontSizeDip -
                                                    default_font.GetFontSize())
                               .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  text_label_->SetMultiLine(/*multi_line=*/true);
  text_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  text_label_->SetText(l10n_util::GetStringUTF16(description_string_id_));

  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosHeadline1,
                                        *text_label_);
  text_label_->SetLineHeight(kTextLabelLineHeightDip);

  // Set up layout row for the buttons.
  auto* button_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  button_container->SetProperty(views::kCrossAxisAlignmentKey,
                                views::LayoutAlignment::kEnd);
  button_container->SetBetweenChildSpacing(kButtonSpacingDip);
  button_container->SetBorder(
      views::CreateEmptyBorder(kButtonContainerBorderInsets));
  dismiss_button_ = button_container->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&SubFeatureOptInView::DismissButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_SUB_FEATURE_OPT_IN_DISMISS_BUTTON),
      PillButton::Type::kFloatingWithoutIcon, /*icon=*/nullptr));
  dismiss_button_->SetID(kSubFeatureOptInDismissButton);
  dismiss_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(dismiss_button_accessible_name_string_id_));
  set_up_button_ = button_container->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&SubFeatureOptInView::SetUpButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_NOTIFICATION_OPT_IN_SET_UP_BUTTON),
      PillButton::Type::kPrimaryWithoutIcon, /*icon=*/nullptr));
  set_up_button_->SetID(kSubFeatureOptInConfirmButton);
  set_up_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(set_up_button_accessible_name_string_id_));

  // By default, the description will be set to the tooltip text, but the title
  // is already announced in the accessible name.
  set_up_button_->GetViewAccessibility().SetDescription(
      u"", ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  dismiss_button_->GetViewAccessibility().SetDescription(
      u"", ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
}

BEGIN_METADATA(SubFeatureOptInView)
END_METADATA

}  // namespace ash
