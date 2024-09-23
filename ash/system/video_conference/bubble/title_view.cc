// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/title_view.h"

#include <memory>

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
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash::video_conference {

namespace {

constexpr gfx::Size kIconSize{20, 20};
constexpr char kSidetoneNudgeId[] = "video_conference_tray_nudge_ids.sidetone";
constexpr auto kTitleChildSpacing = 8;
constexpr auto kTitleViewPadding = gfx::Insets::TLBR(16, 16, 0, 16);
constexpr auto kMicTestButtonPadding = gfx::Insets::TLBR(6, 6, 6, 6);

}  // namespace

TitleView::TitleView(base::OnceClosure close_bubble_callback) {
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

  if (features::IsVcTrayTitleHeaderEnabled()) {
    AddChildView(std::make_unique<MicTestButton>());
    VideoConferenceTrayController::Get()->UpdateSidetoneSupportedState();
  }

  if (features::IsVcStudioLookEnabled()) {
    AddChildView(
        std::make_unique<SettingsButton>(std::move(close_bubble_callback)));
  }
}

TitleView::~TitleView() {
  auto* controller = VideoConferenceTrayController::Get();
  if (controller->GetSidetoneEnabled()) {
    controller->SetSidetoneEnabled(false);
  }
}

BEGIN_METADATA(TitleView)
END_METADATA

MicTestButton::MicTestButton() {
  background_view_ = AddChildView(std::make_unique<View>());
  SetLayoutManager(std::make_unique<views::FillLayout>());
  background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  auto* background_layer = background_view_->layer();
  background_layer->SetRoundedCornerRadius(gfx::RoundedCornersF(16));
  background_layer->SetFillsBoundsOpaquely(false);

  AddChildView(std::make_unique<MicTestButtonContainer>(base::BindRepeating(
      &MicTestButton::OnMicTestButtonClicked, base::Unretained(this))));
}

void MicTestButton::OnThemeChanged() {
  views::View::OnThemeChanged();

  SkColor color = GetColorProvider()->GetColor(
      VideoConferenceTrayController::Get()->GetSidetoneEnabled()
          ? cros_tokens::kCrosSysSystemPrimaryContainer
          : cros_tokens::kCrosSysSystemOnBase);
  background_view_->layer()->SetColor(color);
}

void MicTestButton::OnMicTestButtonClicked(const ui::Event& event) {
  auto* controller = VideoConferenceTrayController::Get();
  const bool enabled = !controller->GetSidetoneEnabled();

  if (enabled) {
    const bool supported = controller->IsSidetoneSupported();
    ShowSidetoneBubble(supported);

    if (supported) {
      controller->SetSidetoneEnabled(enabled);
    }
  } else {
    CloseSidetoneBubble();
    controller->SetSidetoneEnabled(enabled);
  }

  OnThemeChanged();
}

void MicTestButton::ShowSidetoneBubble(const bool supported) {
  NudgeCatalogName catalog_name =
      supported ? NudgeCatalogName::kVideoConferenceTraySidetoneEnabled
                : NudgeCatalogName::kVideoConferenceTraySidetoneNotSupported;

  std::u16string body_str = l10n_util::GetStringUTF16(
      supported ? IDS_ASH_VIDEO_CONFERENCE_SIDETONE_ENABLED_BUBBLE_BODY
                : IDS_ASH_VIDEO_CONFERENCE_SIDETONE_NOT_SUPPORTED_BUBBLE_BODY);

  AnchoredNudgeData nudge_data(kSidetoneNudgeId, catalog_name, body_str,
                               /*anchor_view=*/this);
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

void MicTestButton::CloseSidetoneBubble() {
  auto* nudge_manager = AnchoredNudgeManager::Get();
  if (nudge_manager) {
    nudge_manager->Cancel(kSidetoneNudgeId);
  }
}

MicTestButton::~MicTestButton() = default;

BEGIN_METADATA(MicTestButton)
END_METADATA

MicTestButtonContainer::MicTestButtonContainer(PressedCallback callback)
    : Button(std::move(callback)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  sidetone_icon_ = AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(ui::ImageModel::FromVectorIcon(
              kVideoConferenceSidetoneIcon, cros_tokens::kCrosSysOnSurface))
          .SetImageSize(kIconSize)
          .Build());
  if (features::IsVcTrayMicIndicatorEnabled()) {
    mic_indicator_ = AddChildView(std::make_unique<MicIndicator>());
  }

  SetBorder(views::CreateEmptyBorder(kMicTestButtonPadding));
  // Paints this view to a layer so it will be on top of the
  // `background_view_` of MicTestButton.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_BUBBLE_SIDETONE_TOGGLE_TOOLTIP));
  SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_BUBBLE_SIDETONE_TOGGLE_TOOLTIP));
}

MicTestButtonContainer::~MicTestButtonContainer() = default;

BEGIN_METADATA(MicTestButtonContainer)
END_METADATA

}  // namespace ash::video_conference
