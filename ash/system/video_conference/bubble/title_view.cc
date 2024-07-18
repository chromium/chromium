// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/title_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash::video_conference {

namespace {

constexpr gfx::Size kIconSize{20, 20};
constexpr auto kTitleChildSpacing = 8;
constexpr auto kTitleViewPadding = gfx::Insets::TLBR(16, 16, 0, 16);

}  // namespace

TitleView::TitleView() {
  SetOrientation(views::LayoutOrientation::kHorizontal);
  SetInsideBorderInsets(kTitleViewPadding);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* title_column =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetBetweenChildSpacing(kTitleChildSpacing)
                       .Build());

  title_column->AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(ui::ImageModel::FromVectorIcon(
              // TODO(b/353775770): change icon
              kPrivacyIndicatorsCameraIcon, cros_tokens::kCrosSysOnSurface))
          .SetImageSize(kIconSize)
          .Build());

  auto* title_label = title_column->AddChildView(
      views::Builder<views::Label>()
          // TODO(b/353775770): change label
          .SetText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_IME))
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetEnabledColorId(kColorAshTextColorPrimary)
          .SetAutoColorReadabilityEnabled(false)
          .Build());

  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle1,
                                        *title_label);
  SetFlexForView(title_column, 1);

  auto* mic_test_column =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetVisible(VideoConferenceTrayController::Get()
                                       ->GetHasMicrophonePermissions())
                       .Build());

  sidetone_button_ = mic_test_column->AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&TitleView::OnSidetoneButtonClicked,
                          weak_ptr_factory_.GetWeakPtr()),
      IconButton::Type::kMedium,
      // TODO(b/353775770): change icon and text
      &kPrivacyIndicatorsMicrophoneIcon,
      VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE, /*is_toggleable=*/true,
      /*has_border=*/false));

  sidetone_button_->SetBackgroundColor(SK_ColorTRANSPARENT);
  sidetone_button_->SetBackgroundToggledColor(
      cros_tokens::kCrosSysSystemPrimaryContainer);
  sidetone_button_->SetToggled(
      VideoConferenceTrayController::Get()->GetSidetoneEnabled());
}

void TitleView::OnSidetoneButtonClicked(const ui::Event& event) {
  auto* controller = VideoConferenceTrayController::Get();
  const bool enabled = !controller->GetSidetoneEnabled();
  sidetone_button_->SetToggled(enabled);
  controller->SetSidetoneEnabled(enabled);
}

TitleView::~TitleView() {
  auto* controller = VideoConferenceTrayController::Get();
  if (controller->GetSidetoneEnabled()) {
    controller->SetSidetoneEnabled(false);
  }
}

BEGIN_METADATA(TitleView)
END_METADATA

}  // namespace ash::video_conference
