// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/educational_view.h"

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/constants/ash_features.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"

namespace arc::input_overlay {

namespace {
// Full view size.
constexpr int kDialogWidthMax = 416;
constexpr int kDialogWidthMin = 100;
constexpr int kDialogMarginMin = 24;

// About title style.
constexpr int kDialogShadowElevation = 3;
constexpr int kDialogCornerRadius = 12;

constexpr int kTitleFontSizeLandscape = 28;
constexpr int kTitleFontSizePortrait = 18;

// About description style.
constexpr int kDescriptionFontSize = 13;

// About Alpha style.
constexpr int kAlphaFontSize = 10;
constexpr int kAlphaCornerRadius = 4;
constexpr int kAlphaHeight = 20;
constexpr int kAlphaSidePadding = 4;
constexpr int kAlphaLeftMargin = 8;

// Misc spacing.
constexpr int kBorderRowLandscape1 = 16;
constexpr int kBorderRowLandscape2 = 20;
constexpr int kBorderRowLandscape3 = 32;
constexpr int kBorderRowLandscape4 = 36;
constexpr int kBorderSidesLandscape = 40;

// Phone size.
constexpr int kBorderRowPortrait1 = 16;
constexpr int kBorderRowPortrait2 = 12;
constexpr int kBorderRowPortrait3 = 24;
constexpr int kBorderRowPortrait4 = 28;
constexpr int kBorderSidesPortrait = 32;

// About focus ring.
// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -4;
// Thickness of focus ring.
constexpr float kHaloThickness = 2;

// Helper methods to retrieve the right dimensions/font-sizes.
int GetBorderRow1(bool portrait_mode) {
  return portrait_mode ? kBorderRowPortrait1 : kBorderRowLandscape1;
}

int GetBorderRow2(bool portrait_mode) {
  return portrait_mode ? kBorderRowPortrait2 : kBorderRowLandscape2;
}

int GetBorderRow3(bool portrait_mode) {
  return portrait_mode ? kBorderRowPortrait3 : kBorderRowLandscape3;
}

int GetBorderRow4(bool portrait_mode) {
  return portrait_mode ? kBorderRowPortrait4 : kBorderRowLandscape4;
}

int GetBorderSides(bool portrait_mode) {
  return portrait_mode ? kBorderSidesPortrait : kBorderSidesLandscape;
}

int GetDialogWidth(int parent_width) {
  if (parent_width < kDialogWidthMax + 2 * kDialogMarginMin) {
    return std::max(kDialogWidthMin, parent_width - 2 * kDialogMarginMin);
  }

  return kDialogWidthMax;
}

int GetTitleFontSize(bool portrait_mode) {
  return portrait_mode ? kTitleFontSizePortrait : kTitleFontSizeLandscape;
}

void SetBanner(views::ImageView& image) {
  image.SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      ash::DarkLightModeController::Get()->IsDarkModeEnabled()
          ? IDR_ARC_INPUT_OVERLAY_ONBOARDING_ILLUSTRATION_DARK_JSON
          : IDR_ARC_INPUT_OVERLAY_ONBOARDING_ILLUSTRATION_LIGHT_JSON));
}

}  // namespace

// static
EducationalView* EducationalView::Show(
    DisplayOverlayController* display_overlay_controller,
    views::View* parent) {
  auto educational_view = std::make_unique<EducationalView>(
      display_overlay_controller, parent->width());
  educational_view->Init(parent->size());
  auto* view_ptr = parent->AddChildView(std::move(educational_view));
  view_ptr->AddShadow();

  return view_ptr;
}

EducationalView::EducationalView(
    DisplayOverlayController* display_overlay_controller,
    int parent_width)
    : display_overlay_controller_(display_overlay_controller) {
  portrait_mode_ = parent_width < kDialogWidthMax;
}

EducationalView::~EducationalView() {}

void EducationalView::OnThemeChanged() {
  views::View::OnThemeChanged();
  DCHECK(banner_);
  SetBanner(*banner_);
}

void EducationalView::Init(const gfx::Size& parent_size) {
  DCHECK(display_overlay_controller_);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  SetBackground(views::CreateThemedRoundedRectBackground(
      ash::kColorAshDialogBackgroundColor, kDialogCornerRadius));

  const bool is_dark = ash::DarkLightModeController::Get()->IsDarkModeEnabled();
  const int parent_width = parent_size.width();
  {
    // UI's banner.
    auto banner = std::make_unique<views::ImageView>();
    SetBanner(*banner);

    if (portrait_mode_) {
      // Resize the banner image size proportionally.
      const auto size = banner->CalculatePreferredSize({});
      const int width =
          GetDialogWidth(parent_width) - GetBorderSides(portrait_mode_) * 2;
      const float ratio = 1.0 * width / size.width();
      banner->SetImageSize(gfx::Size(width, size.height() * ratio));
    }
    banner_ = AddChildView(std::move(banner));
  }
  {
    // `Game controls [Alpha]` title tag.
    auto container_view = std::make_unique<views::View>();
    container_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
    // Game controls.
    container_view->AddChildView(
        ash::login_views_utils::CreateThemedBubbleLabel(
            l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_GAME_CONTROLS_ALPHA),
            /*view_defining_max_width=*/nullptr,
            /*enabled_color_type=*/
            is_dark ? cros_tokens::kTextColorPrimary
                    : cros_tokens::kTextColorPrimaryLight,
            /*font_list=*/
            gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                          gfx::Font::FontStyle::NORMAL,
                          GetTitleFontSize(portrait_mode_),
                          gfx::Font::Weight::MEDIUM)));

    auto* alpha_label = container_view->AddChildView(
        ash::login_views_utils::CreateThemedBubbleLabel(
            l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_RELEASE_ALPHA),
            /*view_defining_max_width=*/nullptr,
            /*enabled_color_type=*/
            cros_tokens::kCrosSysPrimary,
            /*font_list=*/
            gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                          gfx::Font::FontStyle::NORMAL, kAlphaFontSize,
                          gfx::Font::Weight::MEDIUM)));
    alpha_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    alpha_label->SetPreferredSize(gfx::Size(
        alpha_label->GetPreferredSize().width() + 2 * kAlphaSidePadding,
        kAlphaHeight));
    alpha_label->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysHighlightShape, kAlphaCornerRadius));
    alpha_label->SetProperty(views::kMarginsKey,
                             gfx::Insets::TLBR(0, kAlphaLeftMargin, 0, 0));
    container_view->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(GetBorderRow1(portrait_mode_),
                                              GetBorderSides(portrait_mode_), 0,
                                              GetBorderSides(portrait_mode_)));
    AddChildView(std::move(container_view));
  }
  {
    // Feature's description text.
    auto* description_label =
        AddChildView(ash::login_views_utils::CreateThemedBubbleLabel(
            l10n_util::GetStringUTF16(
                IDS_INPUT_OVERLAY_EDUCATIONAL_DESCRIPTION_ALPHA),
            /*view_defining_max_width=*/nullptr,
            /*enabled_color_type=*/
            is_dark ? cros_tokens::kTextColorPrimary
                    : cros_tokens::kTextColorPrimaryLight,
            /*font_list=*/
            gfx::FontList({ash::login_views_utils::kGoogleSansFont},
                          gfx::Font::FontStyle::NORMAL, kDescriptionFontSize,
                          gfx::Font::Weight::MEDIUM)));
    description_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_CENTER);
    description_label->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(GetBorderRow2(portrait_mode_),
                                              GetBorderSides(portrait_mode_),
                                              GetBorderRow3(portrait_mode_),
                                              GetBorderSides(portrait_mode_)));
    description_label->SetMultiLine(true);
    description_label->SetMaximumWidth(GetDialogWidth(parent_width) -
                                       2 * GetBorderSides(portrait_mode_));
    description_label->SetSize(gfx::Size());
  }
  {
    // Edit/add `Got it` button to exit UI.
    accept_button_ = AddChildView(std::make_unique<ash::PillButton>(
        base::BindRepeating(&EducationalView::OnAcceptedPressed,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDUCATIONAL_ACCEPT_BUTTON),
        ash::PillButton::Type::kDefaultWithoutIcon,
        /*icon=*/nullptr));
    accept_button_->SetButtonTextColor(cros_styles::ResolveColor(
        cros_styles::ColorName::kButtonLabelColorPrimary, IsDarkModeEnabled()));
    accept_button_->SetBackgroundColor(cros_styles::ResolveColor(
        cros_styles::ColorName::kButtonBackgroundColorPrimary,
        IsDarkModeEnabled()));
    ash::StyleUtil::SetUpInkDropForButton(accept_button_, gfx::Insets(),
                                          /*highlight_on_hover=*/true,
                                          /*highlight_on_focus=*/true);
    auto* focus_ring = views::FocusRing::Get(accept_button_);
    focus_ring->SetHaloInset(kHaloInset);
    focus_ring->SetHaloThickness(kHaloThickness);
    focus_ring->SetColorId(ui::kColorAshFocusRing);
  }
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, 0, GetBorderRow4(portrait_mode_), 0)));
  const auto ui_size = GetPreferredSize();
  SetSize(ui_size);
  SetPosition(gfx::Point((parent_size.width() - ui_size.width()) / 2,
                         (parent_size.height() - ui_size.height()) / 2));
}

void EducationalView::AddShadow() {
  view_shadow_ =
      std::make_unique<views::ViewShadow>(this, kDialogShadowElevation);
  view_shadow_->SetRoundedCornerRadius(kDialogCornerRadius);
}

void EducationalView::OnAcceptedPressed() {
  display_overlay_controller_->OnEducationalViewDismissed();
}

BEGIN_METADATA(EducationalView)
END_METADATA

}  // namespace arc::input_overlay
