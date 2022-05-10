// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/educational_view.h"

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "base/bind.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/flex_layout.h"

namespace arc {
namespace input_overlay {

namespace {
// General menu size.
constexpr int kMenuWidth = 416;
constexpr int kMenuHeight = 380;

// Misc spacing.
constexpr int kBorderRow1 = 16;
constexpr int kBorderRow2 = 20;
constexpr int kBorderRow3 = 32;
constexpr int kBorderRow4 = 36;
constexpr int kBorderSides = 40;
constexpr int kBlankCol = 12;

// Fonts sizes, styles.
constexpr int kTitleFontSize = 20;
constexpr int kBetaFontSize = 11;
constexpr int kDescriptionFontSize = 13;
constexpr int kCornerRadius = 12;
}  // namespace

// static
std::unique_ptr<EducationalView> EducationalView::BuildMenu(
    DisplayOverlayController* display_overlay_controller,
    views::View* parent) {
  auto educational_view =
      std::make_unique<EducationalView>(display_overlay_controller);
  educational_view->Init(parent);

  return educational_view;
}

EducationalView::EducationalView(
    DisplayOverlayController* display_overlay_controller)
    : display_overlay_controller_(display_overlay_controller) {}

EducationalView::~EducationalView() {}

void EducationalView::Init(views::View* parent) {
  DCHECK(parent);
  DCHECK(display_overlay_controller_);
  DCHECK(ash::AshColorProvider::Get());
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  auto* color_provider = ash::AshColorProvider::Get();
  // TODO(djacobo): Unspecified color for UI's background, it doesn't match
  // system's background so need to confirm color.
  SetBackground(
      views::CreateRoundedRectBackground(SK_ColorWHITE, kCornerRadius));

  {
    // UI's banner.
    const gfx::ImageSkia* skia_banner =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDS_ARC_INPUT_OVERLAY_ONBOARDING_ILLUSTRATION);
    CHECK(skia_banner);
    auto banner = std::make_unique<views::ImageView>(
        ui::ImageModel::FromImageSkia(*skia_banner));
    banner->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kBorderRow4, kBorderRow4, kBorderRow1, kBorderRow4)));
    AddChildView(std::move(banner));
  }
  {
    // |Game Control [beta]| tittle tag.
    auto container_view = std::make_unique<views::View>();
    container_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    // TODO(djacobo): Although this is using |kTextColorPrimary| as in specs the
    // color does not correspond to what specs visually look like.
    SkColor color_primary = color_provider->GetContentLayerColor(
        ash::AshColorProvider::ContentLayerType::kTextColorPrimary);
    auto* game_control = ash::login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDUCATIONAL_TITLE),
        /*view_defining_max_width=*/nullptr, color_primary,
        /*font_list=*/
        gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                      gfx::Font::FontStyle::NORMAL, kTitleFontSize,
                      gfx::Font::Weight::MEDIUM));
    container_view->AddChildView(std::move(game_control));

    SkColor color_beta =
        arc::GetCrOSColor(cros_styles::ColorName::kTextColorSelection);
    auto* beta_label = ash::login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDUCATIONAL_BETA),
        /*view_defining_max_width=*/nullptr, color_beta,
        /*font_list=*/
        gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                      gfx::Font::FontStyle::NORMAL, kBetaFontSize,
                      gfx::Font::Weight::MEDIUM));
    beta_label->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(0, kBlankCol, 0, 0)));
    beta_label->SetBackgroundColor(
        arc::GetCrOSColor(cros_styles::ColorName::kHighlightColor));
    container_view->AddChildView(std::move(beta_label));
    container_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kBorderRow1, kBorderSides, 0, kBorderSides)));
    AddChildView(std::move(container_view));
  }
  {
    // Feature's description text.
    SkColor color_secondary = color_provider->GetContentLayerColor(
        ash::AshColorProvider::ContentLayerType::kTextColorSecondary);
    auto* description_label = ash::login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDUCATIONAL_DESCRIPTION),
        /*view_defining_max_width=*/this, color_secondary,
        /*font_list=*/
        gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                      gfx::Font::FontStyle::NORMAL, kDescriptionFontSize,
                      gfx::Font::Weight::MEDIUM));
    description_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_CENTER);
    description_label->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        kBorderRow2, kBorderSides, kBorderRow3, kBorderSides)));
    description_label->SetMultiLine(true);
    description_label->SetSize(gfx::Size());
    AddChildView(std::move(description_label));
  }
  {
    // Edit/add |Got it| button to exit UI.
    accept_button_ = AddChildView(std::make_unique<ash::PillButton>(
        base::BindRepeating(&EducationalView::OnAcceptedPressed,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDUCATIONAL_ACCEPT_BUTTON),
        ash::PillButton::Type::kIconless,
        /*icon=*/nullptr));
    accept_button_->SetBackgroundColor(gfx::kGoogleBlue300);
  }
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, kBorderRow4, 0)));
  const auto ui_size = GetPreferredSize();
  SetSize(ui_size);
  const auto parent_size = parent->size();
  SetPosition(gfx::Point((parent_size.width() - ui_size.width()) / 2,
                         (parent_size.height() - ui_size.height()) / 2));
}

void EducationalView::OnAcceptedPressed() {
  display_overlay_controller_->OnEducationalViewDismissed();
}

gfx::Size EducationalView::CalculatePreferredSize() const {
  // TODO(djacobo): This is needed as in portrait mode width() may be smaller
  // than the banner at the top. Compare against specs and this.parent() size.
  auto available_size = View::CalculatePreferredSize();
  auto spec_size = gfx::Size(kMenuWidth, kMenuHeight);

  return gfx::Size(std::min(available_size.width(), spec_size.width()),
                   std::min(available_size.height(), spec_size.height()));
}

}  // namespace input_overlay
}  // namespace arc
