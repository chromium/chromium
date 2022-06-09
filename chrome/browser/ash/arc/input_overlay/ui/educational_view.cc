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
// About the dialog view style.
constexpr int kDialogWidth = 416;
constexpr int kDialogHeight = 380;
constexpr int kDialogShadowElevation = 3;
constexpr int kDialogCornerRadius = 12;

// About title style.
constexpr int kTitleFontSize = 20;

// About description style.
constexpr int kDescriptionFontSize = 13;

// About Alpha style.
constexpr int kAlphaFontSize = 11;
constexpr int kAlphaCornerRadius = 4;
constexpr int kAlphaHeight = 16;
constexpr int kAlphaSidePadding = 4;
constexpr int kAlphaLeftMargin = 12;

// Misc spacing.
constexpr int kBorderRow1 = 16;
constexpr int kBorderRow2 = 20;
constexpr int kBorderRow3 = 32;
constexpr int kBorderRow4 = 36;
constexpr int kBorderSides = 40;
}  // namespace

// static
EducationalView* EducationalView::Show(
    DisplayOverlayController* display_overlay_controller,
    views::View* parent) {
  auto educational_view =
      std::make_unique<EducationalView>(display_overlay_controller);
  educational_view->Init(parent);
  auto* view_ptr = parent->AddChildView(std::move(educational_view));
  view_ptr->AddShadow();

  return view_ptr;
}

EducationalView::EducationalView(
    DisplayOverlayController* display_overlay_controller)
    : display_overlay_controller_(display_overlay_controller) {}

EducationalView::~EducationalView() {}

void EducationalView::Init(views::View* parent) {
  DCHECK(parent);
  DCHECK(display_overlay_controller_);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  SetBackground(views::CreateRoundedRectBackground(
      GetDialogBackgroundBaseColor(), kDialogCornerRadius));

  {
    // UI's banner.
    const gfx::ImageSkia* skia_banner =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDS_ARC_INPUT_OVERLAY_ONBOARDING_ILLUSTRATION);
    CHECK(skia_banner);
    auto banner = std::make_unique<views::ImageView>(
        ui::ImageModel::FromImageSkia(*skia_banner));
    banner->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(kBorderRow4, kBorderRow4, kBorderRow1, kBorderRow4));
    AddChildView(std::move(banner));
  }
  {
    // |Game controls [Alpha]| title tag.
    auto container_view = std::make_unique<views::View>();
    container_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    auto* game_control = ash::login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDUCATIONAL_TITLE_ALPHA),
        /*view_defining_max_width=*/nullptr,
        /*color=*/
        GetContentLayerColor(
            ash::AshColorProvider::ContentLayerType::kTextColorPrimary),
        /*font_list=*/
        gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                      gfx::Font::FontStyle::NORMAL, kTitleFontSize,
                      gfx::Font::Weight::MEDIUM));
    container_view->AddChildView(std::move(game_control));

    auto* alpha_label = ash::login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_RELEASE_ALPHA),
        /*view_defining_max_width=*/nullptr, /*color=*/
        arc::GetCrOSColor(cros_styles::ColorName::kTextColorSelection),
        /*font_list=*/
        gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                      gfx::Font::FontStyle::NORMAL, kAlphaFontSize,
                      gfx::Font::Weight::MEDIUM));
    alpha_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    alpha_label->SetPreferredSize(gfx::Size(
        alpha_label->GetPreferredSize().width() + 2 * kAlphaSidePadding,
        kAlphaHeight));
    alpha_label->SetBackground(views::CreateRoundedRectBackground(
        arc::GetCrOSColor(cros_styles::ColorName::kHighlightColor),
        kAlphaCornerRadius));
    alpha_label->SetProperty(views::kMarginsKey,
                             gfx::Insets::TLBR(0, kAlphaLeftMargin, 0, 0));
    container_view->AddChildView(std::move(alpha_label));
    container_view->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(kBorderRow1, kBorderSides, 0, kBorderSides));
    AddChildView(std::move(container_view));
  }
  {
    // Feature's description text.
    auto* description_label = ash::login_views_utils::CreateBubbleLabel(
        l10n_util::GetStringUTF16(
            IDS_INPUT_OVERLAY_EDUCATIONAL_DESCRIPTION_ALPHA),
        /*view_defining_max_width=*/this,
        /*color=*/
        GetContentLayerColor(
            ash::AshColorProvider::ContentLayerType::kTextColorSecondary),
        /*font_list=*/
        gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                      gfx::Font::FontStyle::NORMAL, kDescriptionFontSize,
                      gfx::Font::Weight::MEDIUM));
    description_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_CENTER);
    description_label->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(kBorderRow2, kBorderSides,
                                              kBorderRow3, kBorderSides));
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
    accept_button_->SetBackgroundColor(GetContentLayerColor(
        ash::AshColorProvider::ContentLayerType::kButtonLabelColorBlue));
  }
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, kBorderRow4, 0)));
  const auto ui_size = GetPreferredSize();
  SetSize(ui_size);
  const auto parent_size = parent->size();
  SetPosition(gfx::Point((parent_size.width() - ui_size.width()) / 2,
                         (parent_size.height() - ui_size.height()) / 2));
}

void EducationalView::AddShadow() {
  view_shadow_ =
      std::make_unique<ash::ViewShadow>(this, kDialogShadowElevation);
  view_shadow_->SetRoundedCornerRadius(kDialogCornerRadius);
}

void EducationalView::OnAcceptedPressed() {
  display_overlay_controller_->OnEducationalViewDismissed();
}

gfx::Size EducationalView::CalculatePreferredSize() const {
  // TODO(djacobo): This is needed as in portrait mode width() may be smaller
  // than the banner at the top. Compare against specs and this.parent() size.
  auto available_size = View::CalculatePreferredSize();
  auto spec_size = gfx::Size(kDialogWidth, kDialogHeight);

  return gfx::Size(std::min(available_size.width(), spec_size.width()),
                   std::min(available_size.height(), spec_size.height()));
}

}  // namespace input_overlay
}  // namespace arc
