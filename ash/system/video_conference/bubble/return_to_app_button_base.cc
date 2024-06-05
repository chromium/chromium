// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/return_to_app_button_base.h"

#include <memory>
#include <string>

#include "ash/bubble/bubble_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash::video_conference {

namespace {

const int kReturnToAppIconsContainerSpacing = 2;

// Creates a view containing camera, microphone, and screen share icons that
// shows capturing state of a media app.
std::unique_ptr<views::View> CreateReturnToAppIconsContainer(
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen) {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, kReturnToAppIconsContainerSpacing / 2, 0,
                                    kReturnToAppIconsContainerSpacing / 2));

  if (is_capturing_camera) {
    auto camera_icon = std::make_unique<views::ImageView>();
    camera_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsCameraIcon, cros_tokens::kCrosSysPositive,
        kReturnToAppIconSize));
    container->AddChildView(std::move(camera_icon));
  }

  if (is_capturing_microphone) {
    auto microphone_icon = std::make_unique<views::ImageView>();
    microphone_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsMicrophoneIcon, cros_tokens::kCrosSysPositive,
        kReturnToAppIconSize));
    container->AddChildView(std::move(microphone_icon));
  }

  if (is_capturing_screen) {
    auto screen_share_icon = std::make_unique<views::ImageView>();
    screen_share_icon->SetImage(ui::ImageModel::FromVectorIcon(
        kPrivacyIndicatorsScreenShareIcon, cros_tokens::kCrosSysPositive,
        kReturnToAppIconSize));
    container->AddChildView(std::move(screen_share_icon));
  }

  return container;
}

}  // namespace

ReturnToAppButtonBase::ReturnToAppButtonBase(
    const base::UnguessableToken& id,
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen,
    const std::u16string& display_text,
    crosapi::mojom::VideoConferenceAppType app_type)
    : is_capturing_camera_(is_capturing_camera),
      is_capturing_microphone_(is_capturing_microphone),
      is_capturing_screen_(is_capturing_screen) {
  SetCallback(base::BindRepeating(&ReturnToAppButtonBase::OnButtonClicked,
                                  weak_ptr_factory_.GetWeakPtr(), id,
                                  app_type));

  icons_container_ = AddChildView(CreateReturnToAppIconsContainer(
      is_capturing_camera, is_capturing_microphone, is_capturing_screen));

  auto label = std::make_unique<views::Label>(display_text);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));

  label->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *label);
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

  label_ = AddChildView(std::move(label));
  GetViewAccessibility().SetName(GetPeripheralsAccessibleName() + display_text);
}

ReturnToAppButtonBase::~ReturnToAppButtonBase() = default;

void ReturnToAppButtonBase::OnButtonClicked(
    const base::UnguessableToken& id,
    crosapi::mojom::VideoConferenceAppType app_type) {
  ash::VideoConferenceTrayController::Get()->ReturnToApp(id);
  base::UmaHistogramEnumeration("Ash.VideoConference.ReturnToApp.Click",
                                app_type);
}

std::u16string ReturnToAppButtonBase::GetPeripheralsAccessibleName() const {
  std::u16string tooltip_text;
  if (is_capturing_camera_) {
    tooltip_text += l10n_util::GetStringFUTF16(
        VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
        l10n_util::GetStringUTF16(VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_CAMERA));
  }
  if (is_capturing_microphone_) {
    tooltip_text += l10n_util::GetStringFUTF16(
        VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
        l10n_util::GetStringUTF16(
            VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE));
  }
  if (is_capturing_screen_) {
    tooltip_text += l10n_util::GetStringFUTF16(
        VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
        l10n_util::GetStringUTF16(
            VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_SCREEN_SHARE));
  }
  return tooltip_text;
}

std::u16string ReturnToAppButtonBase::GetLabelText() const {
  return label_->GetText();
}

BEGIN_METADATA(ReturnToAppButtonBase)
END_METADATA

}  // namespace ash::video_conference
