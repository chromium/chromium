// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/disclaimer_view.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

namespace {

// Paddings, sizes and insets.
constexpr int kBetweenButtonsSpacing = 8;
constexpr int kButtonHeight = 32;
constexpr int kContainerBottomPadding = 28;
constexpr int kContainerPadding = 32;
constexpr int kImageHeight = 236;
constexpr int kImageWidth = 512;
constexpr int kWidgetWidth = kImageWidth;
constexpr int kRadius = 20;
constexpr int kTextContainerBetweenChildSpacing = 16;
constexpr int kWidgetMiniumHeight = 650;
constexpr gfx::Insets kButtonContainerInsets =
    gfx::Insets::TLBR(0,
                      kContainerPadding,
                      kContainerBottomPadding,
                      kContainerPadding);
constexpr gfx::Insets kTextContainerInsets = gfx::Insets(kContainerPadding);
constexpr gfx::Size kImagePreferredSize(/*width=*/kImageWidth,
                                        /*height=*/kImageHeight);

std::u16string GetTextTitle() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_CAPTURE_SEARCH_SAMPLE_DISCLAIMER_TITLE);
#else
  return u"Disclaimer title";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetTextAcceptButton() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_CAPTURE_SEARCH_SAMPLE_DISCLAIMER_ACCEPT);
#else
  return u"Accept Button";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetTextDeclineButton() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(
      IDS_CAPTURE_SEARCH_SAMPLE_DISCLAIMER_DECLINE);
#else
  return u"Decline Button";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetBodyTextParagraphOne() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(
      IDS_CAPTURE_SEARCH_SAMPLE_DISCLAIMER_PARAGRAPH_ONE);
#else
  return u"This is the disclaimer view for a capture mode feature.";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetBodyTextParagraphTwo() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(
      IDS_CAPTURE_SEARCH_SAMPLE_DISCLAIMER_PARAGRAPH_TWO);
#else
  return u"Read the terms and conditions.";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

views::Builder<views::StyledLabel> GetTextBodyBuilder(
    const std::u16string& text) {
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosBody1);
  style.override_color_id = static_cast<ui::ColorId>(ui::kColorSysOnSurface);

  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .AddStyleRange(gfx::Range(0, text.length()), style)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
}

}  // namespace

DisclaimerView::DisclaimerView(
    base::RepeatingClosure press_accept_button_callback,
    base::RepeatingClosure press_decline_button_callback) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysDialogContainer, kRadius));
  SetPaintToLayer();
  AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_MAGIC_BOOST_DISCLAIMER_ILLUSTRATION))
          .SetPreferredSize(kImagePreferredSize)
          .Build());

  AddChildView(
      views::Builder<views::ScrollView>()
          .SetBackgroundColor(std::nullopt)
          .SetDrawOverflowIndicator(false)
          .SetVerticalScrollBarMode(
              views::ScrollView::ScrollBarMode::kHiddenButEnabled)
          // The content will take all the available space of the widget.
          .SetProperty(views::kBoxLayoutFlexKey,
                       views::BoxLayoutFlexSpecification())
          .ClipHeightTo(0, std::numeric_limits<int>::max())
          .SetContents(
              views::Builder<views::BoxLayoutView>()
                  .SetOrientation(views::LayoutOrientation::kVertical)
                  .SetBetweenChildSpacing(kTextContainerBetweenChildSpacing)
                  .SetBorder(views::CreateEmptyBorder(kTextContainerInsets))
                  .AddChildren(
                      views::Builder<views::Label>()
                          .SetFontList(
                              TypographyProvider::Get()->ResolveTypographyToken(
                                  TypographyToken::kCrosDisplay7))
                          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                          .SetHorizontalAlignment(
                              gfx::HorizontalAlignment::ALIGN_LEFT)
                          .SetText(GetTextTitle())
                          .CopyAddressTo(&title_),
                      GetTextBodyBuilder(GetBodyTextParagraphOne()),
                      GetTextBodyBuilder(GetBodyTextParagraphTwo())))
          .Build());

  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetBetweenChildSpacing(kBetweenButtonsSpacing)
          .SetBorder(views::CreateEmptyBorder(kButtonContainerInsets))
          .AddChildren(
              views::Builder<views::MdTextButton>()
                  .SetText(GetTextDeclineButton())
                  .SetAccessibleName(GetTextDeclineButton())
                  .SetMaxSize(gfx::Size(kImageWidth, kButtonHeight))
                  .SetStyle(ui::ButtonStyle::kProminent)
                  .SetCallback(std::move(press_decline_button_callback))
                  .SetID(kDisclaimerViewDeclineButtonId),
              views::Builder<views::MdTextButton>()
                  .SetText(GetTextAcceptButton())
                  .SetAccessibleName(GetTextAcceptButton())
                  .SetMaxSize(gfx::Size(kImageWidth, kButtonHeight))
                  .SetStyle(ui::ButtonStyle::kProminent)
                  .SetCallback(std::move(press_accept_button_callback))
                  .CopyAddressTo(&accept_button_)
                  .SetID(kDisclaimerViewAcceptButtonId))
          .Build()

  );

  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kRadius});
}

DisclaimerView::~DisclaimerView() = default;

// static
std::unique_ptr<views::Widget> DisclaimerView::CreateWidget(
    aura::Window* const root,
    base::RepeatingClosure press_accept_button_callback,
    base::RepeatingClosure press_decline_button_callback) {
  auto disclaimer_view = std::make_unique<DisclaimerView>(
      std::move(press_accept_button_callback),
      std::move(press_decline_button_callback));

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  const gfx::Rect work_area(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root).work_area());
  params.parent = Shell::GetContainer(root, kShellWindowId_OverlayContainer);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.corner_radius = kRadius;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;

  const int widget_height = disclaimer_view->GetPreferredSize().height();
  params.bounds = gfx::Rect(work_area.CenterPoint().x() - kWidgetWidth / 2,
                            work_area.CenterPoint().y() - widget_height / 2,
                            kWidgetWidth, widget_height);

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::move(disclaimer_view));

  return widget;
}

gfx::Size DisclaimerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int body_width = kWidgetWidth - 2 * kContainerPadding;
  int height = kImageHeight + title_->GetHeightForWidth(body_width) +
               accept_button_->GetHeightForWidth(body_width) +
               kContainerPadding * 2 + kContainerBottomPadding +
               kTextContainerBetweenChildSpacing * 4;
  height = std::max(height, kWidgetMiniumHeight);

  return gfx::Size(kWidgetWidth, height);
}

BEGIN_METADATA(DisclaimerView)
END_METADATA

}  // namespace ash
