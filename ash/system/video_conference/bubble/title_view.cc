// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/title_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
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

constexpr auto kBubbleCornerRadius = 16;
constexpr auto kBubbleChildSpacing = 4;
constexpr auto kBubblePadding = gfx::Insets::TLBR(12, 12, 12, 12);
constexpr auto kBubbleArrowOffset = 8;
constexpr auto kBubbleMaxWidth = 250;

constexpr gfx::Size kIconSize{20, 20};
constexpr auto kTitleChildSpacing = 8;
constexpr auto kTitleViewPadding = gfx::Insets::TLBR(16, 16, 0, 16);

gfx::Rect CalculateBubbleBounds(const gfx::Rect& anchor_view_bounds,
                                const gfx::Size bubble_size) {
  // The sidetone bubble will be located on top of the sidetone button
  // with the right side of the sidetone bubble aligned with the center
  // of the button.

  gfx::Point anchor_top_center = anchor_view_bounds.top_center();
  int bubble_x = anchor_top_center.x() - bubble_size.width();
  int bubble_y =
      anchor_top_center.y() - bubble_size.height() - kBubbleArrowOffset;
  gfx::Point bubble_top_right(bubble_x, bubble_y);

  gfx::Rect bubble_bounds(bubble_top_right, bubble_size);

  return bubble_bounds;
}

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
  CloseSidetoneBubble();

  std::u16string title_str = l10n_util::GetStringUTF16(
      supported ? IDS_ASH_VIDEO_CONFERENCE_SIDETONE_ENABLED_BUBBLE_TITLE
                : IDS_ASH_VIDEO_CONFERENCE_SIDETONE_NOT_SUPPORTED_BUBBLE_TITLE);
  std::u16string body_str = l10n_util::GetStringUTF16(
      supported ? IDS_ASH_VIDEO_CONFERENCE_SIDETONE_ENABLED_BUBBLE_BODY
                : IDS_ASH_VIDEO_CONFERENCE_SIDETONE_NOT_SUPPORTED_BUBBLE_BODY);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;

  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.name = "SidetoneBubble";
  params.parent =
      sidetone_button_->GetWidget()->GetNativeWindow()->GetRootWindow();

  auto bubble_widget = std::make_unique<views::Widget>(std::move(params));

  auto rounded_corners = gfx::RoundedCornersF(kBubbleCornerRadius);
  rounded_corners.set_lower_right(0);
  auto bubble_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetBetweenChildSpacing(kBubbleChildSpacing)
          .SetInsideBorderInsets(kBubblePadding)
          .SetBackground(views::CreateThemedRoundedRectBackground(
              cros_tokens::kCrosSysSystemBaseElevated, rounded_corners))
          .Build();

  bubble_view->SetPaintToLayer();
  bubble_view->layer()->SetBackgroundBlur(
      ash::ColorProvider::kBackgroundBlurSigma);
  bubble_view->layer()->SetBackdropFilterQuality(
      ash::ColorProvider::kBackgroundBlurQuality);
  bubble_view->layer()->SetRoundedCornerRadius(rounded_corners);
  bubble_view->layer()->SetFillsBoundsOpaquely(false);

  auto* title = bubble_view->AddChildView(
      views::Builder<views::Label>()
          .SetText(title_str)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetEnabledColorId(kColorAshTextColorPrimary)
          .Build());
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle2, *title);

  auto* body = bubble_view->AddChildView(
      views::Builder<views::Label>()
          .SetText(body_str)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetEnabledColorId(kColorAshTextColorPrimary)
          .SetMultiLine(true)
          .SetMaximumWidth(kBubbleMaxWidth)
          .Build());
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *body);

  gfx::Size bubble_size = bubble_view->GetPreferredSize();
  bubble_widget->SetContentsView(std::move(bubble_view));

  gfx::Rect anchor_view_bounds = sidetone_button_->GetBoundsInScreen();
  gfx::Rect bubble_bounds =
      CalculateBubbleBounds(anchor_view_bounds, bubble_size);
  bubble_widget->SetBounds(bubble_bounds);

  sidetone_bubble_widget_ = std::move(bubble_widget);
  sidetone_bubble_widget_->Show();
  std::u16string announcement = l10n_util::GetStringFUTF16(
      IDS_ASH_VIDEO_CONFERENCE_SIDETONE_BUBBLE_ANNOUNCEMENT, title_str,
      body_str);

  if (supported) {
    GetViewAccessibility().AnnouncePolitely(announcement);
  } else {
    GetViewAccessibility().AnnounceAlert(announcement);
  }
}

void TitleView::CloseSidetoneBubble() {
  if (!sidetone_bubble_widget_ || sidetone_bubble_widget_->IsClosed()) {
    return;
  }

  sidetone_bubble_widget_->Close();
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
