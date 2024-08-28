// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/title_view.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/video_conference/bubble/mic_indicator.h"
#include "ash/system/video_conference/bubble/settings_button.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash::video_conference {

namespace {

constexpr gfx::Size kIconSize{20, 20};
constexpr char kSidetoneNudgeId[] = "video_conference_tray_nudge_ids.sidetone";
constexpr auto kTitleChildSpacing = 8;
constexpr auto kTitleViewPadding = gfx::Insets::TLBR(16, 16, 0, 16);

}  // namespace

TitleView::TitleView() {
  SetOrientation(views::LayoutOrientation::kHorizontal);
  SetInsideBorderInsets(kTitleViewPadding);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBetweenChildSpacing(kTitleChildSpacing);

  auto* title_column =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetBetweenChildSpacing(kTitleChildSpacing)
                       .Build());

  title_column->AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(ui::ImageModel::FromVectorIcon(
              kSystemMenuVideocamIcon, cros_tokens::kCrosSysOnSurface))
          .SetImageSize(kIconSize)
          .Build());

  auto* title_label = title_column->AddChildView(
      views::Builder<views::Label>()
          .SetText(
              l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_BUBBLE_TITLE))
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

  if (features::IsVcTrayMicIndicatorEnabled()) {
    mic_test_column->AddChildView(std::make_unique<MicIndicator>());
  }

  sidetone_button_ = mic_test_column->AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&TitleView::OnSidetoneButtonClicked,
                          weak_ptr_factory_.GetWeakPtr()),
      IconButton::Type::kMedium, &kVideoConferenceSidetoneIcon,
      IDS_ASH_VIDEO_CONFERENCE_BUBBLE_SIDETONE_TOGGLE_TOOLTIP,
      /*is_toggleable=*/true,
      /*has_border=*/false));

  sidetone_button_->SetBackgroundColor(SK_ColorTRANSPARENT);
  sidetone_button_->SetBackgroundToggledColor(
      cros_tokens::kCrosSysSystemPrimaryContainer);
  sidetone_button_->SetToggled(
      VideoConferenceTrayController::Get()->GetSidetoneEnabled());

  VideoConferenceTrayController::Get()->UpdateSidetoneSupportedState();

  if (features::IsVcStudioLookEnabled()) {
    AddChildView(std::make_unique<SettingsButton>());
  }
}

void TitleView::OnSidetoneButtonClicked(const ui::Event& event) {
  auto* controller = VideoConferenceTrayController::Get();
  const bool enabled = !controller->GetSidetoneEnabled();

  if (enabled) {
    const bool supported = controller->IsSidetoneSupported();
    ShowSidetoneBubble(supported);

    if (supported) {
      sidetone_button_->SetToggled(enabled);
      controller->SetSidetoneEnabled(enabled);
    }
  } else {
    CloseSidetoneBubble();

    sidetone_button_->SetToggled(enabled);
    controller->SetSidetoneEnabled(enabled);
  }
}

void TitleView::ShowSidetoneBubble(const bool supported) {
  NudgeCatalogName catalog_name =
      supported ? NudgeCatalogName::kVideoConferenceTraySidetoneEnabled
                : NudgeCatalogName::kVideoConferenceTraySidetoneNotSupported;

  std::u16string body_str = l10n_util::GetStringUTF16(
      supported ? IDS_ASH_VIDEO_CONFERENCE_SIDETONE_ENABLED_BUBBLE_BODY
                : IDS_ASH_VIDEO_CONFERENCE_SIDETONE_NOT_SUPPORTED_BUBBLE_BODY);

  AnchoredNudgeData nudge_data(kSidetoneNudgeId, catalog_name, body_str,
                               sidetone_button_);
  nudge_data.title_text = l10n_util::GetStringUTF16(
      supported ? IDS_ASH_VIDEO_CONFERENCE_SIDETONE_ENABLED_BUBBLE_TITLE
                : IDS_ASH_VIDEO_CONFERENCE_SIDETONE_NOT_SUPPORTED_BUBBLE_TITLE);
  ;
  nudge_data.announce_chromevox = false;
  nudge_data.set_anchor_view_as_parent = true;
  AnchoredNudgeManager::Get()->Show(nudge_data);

  // AnchoredNudge announcement doesn't have a separator between the title
  // and the body. Use a custom text that includes a separator to make an
  // announcement.
  std::u16string announcement = l10n_util::GetStringFUTF16(
      IDS_ASH_VIDEO_CONFERENCE_SIDETONE_BUBBLE_ANNOUNCEMENT,
      nudge_data.title_text, nudge_data.body_text);

  if (supported) {
    GetViewAccessibility().AnnouncePolitely(announcement);
  } else {
    GetViewAccessibility().AnnounceAlert(announcement);
  }
}

void TitleView::CloseSidetoneBubble() {
  auto* nudge_manager = AnchoredNudgeManager::Get();
  if (nudge_manager) {
    nudge_manager->Cancel(kSidetoneNudgeId);
  }
}

TitleView::~TitleView() {
  auto* controller = VideoConferenceTrayController::Get();
  if (controller->GetSidetoneEnabled()) {
    controller->SetSidetoneEnabled(false);
  }

  CloseSidetoneBubble();
}

BEGIN_METADATA(TitleView)
END_METADATA

}  // namespace ash::video_conference
