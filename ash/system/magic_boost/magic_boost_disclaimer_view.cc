// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/shell.h"
#include "ash/style/typography.h"
#include "ash/system/magic_boost/magic_boost_constants.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

namespace {

constexpr char kWidgetName[] = "MagicBoostDisclaimerViewWidget";

// Paddings, sizes and insets.
constexpr int kImageWidth = 512;
constexpr int kContainerPadding = 32;
constexpr int kTextContainerBetweenChildSpacing = 16;
constexpr int kContainerBottomPadding = 28;
constexpr int kWidgetWidth = kImageWidth;
constexpr int kWidgetHeight = 650;
constexpr int kBetweenButtonsSpacing = 8;
constexpr int kButtonHeight = 32;
constexpr int kRadius = 20;

constexpr gfx::Insets kButtonContainerInsets =
    gfx::Insets::TLBR(0,
                      kContainerPadding,
                      kContainerBottomPadding,
                      kContainerPadding);
constexpr gfx::Insets kTextContainerInsets = gfx::Insets(kContainerPadding);
constexpr gfx::Size kImagePreferredSize(/*width=*/kImageWidth, /*height=*/236);

views::StyledLabel::RangeStyleInfo GetBodyTextStyle() {
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosBody1);
  style.override_color_id = static_cast<ui::ColorId>(ui::kColorSysOnSurface);
  return style;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Placeholder url.
constexpr char kTestURL[] = "https://www.google.com";

// Opens the passed in `url` in a new tab.
void OnLinkClick(const std::string& url) {
  // TODO(b/339044721): open the url in a new tab.
}

views::StyledLabel::RangeStyleInfo GetLinkTextStyle() {
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&OnLinkClick, kTestURL));
  link_style.override_color_id =
      static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurfaceVariant);
  return link_style;
}
#else
// Placeholder texts
// TODO(b/339528642): Replace with real strings.
const std::u16string kTestTitleText = u"Disclaimer title";
const std::u16string kTestSecondaryButtonText = u"No thanks";
const std::u16string kTestPrimaryButtonText = u"Try it";
const std::u16string kTestBodyText =
    u"Body text that is multi-line which means it can span from one line to up "
    u"to three lines for this case.";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

views::Builder<views::StyledLabel> GetTextBodyBuilder(
    const std::u16string& text) {
  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .AddStyleRange(gfx::Range(0, text.length()), GetBodyTextStyle())
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
}

std::u16string GetTitle() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_TITLE);
#else
  return kTestTitleText;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetAcceptButtonText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_ACCEPT_BUTTON);
#else
  return kTestPrimaryButtonText;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetDeclineButtonText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_DECLINE_BUTTON);
#else
  return kTestSecondaryButtonText;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

views::Builder<views::StyledLabel> GetParagraphOneBuilder() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return GetTextBodyBuilder(
      l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAMIER_PARAGRAPH_ONE));
#else
  return GetTextBodyBuilder(kTestBodyText);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

views::Builder<views::StyledLabel> GetParagraphTwoBuilder() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::vector<size_t> offsets;
  const std::u16string link_text =
      l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_TERMS_LINK_TEXT);
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_MAGIC_BOOST_DISCLAIMER_PARAGRAPH_TWO, {link_text}, &offsets);

  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .AddStyleRange(gfx::Range(0, offsets.at(0)), GetBodyTextStyle())
      .AddStyleRange(
          gfx::Range(offsets.at(0), offsets.at(0) + link_text.length()),
          GetLinkTextStyle())
      .AddStyleRange(
          gfx::Range(offsets.at(0) + link_text.length(), text.length()),
          GetBodyTextStyle())
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);

#else
  return GetTextBodyBuilder(kTestBodyText);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

views::Builder<views::StyledLabel> GetParagraphThreeBuilder() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return GetTextBodyBuilder(
      l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_PARAGRAPH_THREE));
#else
  return GetTextBodyBuilder(kTestBodyText);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

views::Builder<views::StyledLabel> GetParagraphFourBuilder() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::vector<size_t> offsets;
  const std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_MAGIC_BOOST_DISCLAIMER_LEARN_MORE_LINK_TEXT);
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_MAGIC_BOOST_DISCLAIMER_PARAGRAPH_FOUR, {link_text}, &offsets);

  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .AddStyleRange(gfx::Range(0, offsets.at(0)), GetBodyTextStyle())
      .AddStyleRange(
          gfx::Range(offsets.at(0), offsets.at(0) + link_text.length()),
          GetLinkTextStyle())
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
#else
  return GetTextBodyBuilder(kTestBodyText);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace

MagicBoostDisclaimerView::MagicBoostDisclaimerView(
    base::RepeatingClosure press_accept_button_callback,
    base::RepeatingClosure press_decline_button_callback) {
  views::View* disclaimer_view;
  views::Builder<views::View>(this)
      .CopyAddressTo(&disclaimer_view)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      .SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysDialogContainer, kRadius))
      .SetPaintToLayer()
      .AddChildren(
          views::Builder<views::ImageView>()
              .SetImage(
                  ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                      IDR_MAGIC_BOOST_DISCLAIMER_ILLUSTRATION))
              .SetPreferredSize(kImagePreferredSize),
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::LayoutOrientation::kVertical)
              .SetProperty(views::kBoxLayoutFlexKey,
                           views::BoxLayoutFlexSpecification())
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
                      .SetText(GetTitle()),
                  GetParagraphOneBuilder(), GetParagraphTwoBuilder(),
                  GetParagraphThreeBuilder(), GetParagraphFourBuilder()),
          views::Builder<views::BoxLayoutView>()
              .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
              .SetBetweenChildSpacing(kBetweenButtonsSpacing)
              .SetBorder(views::CreateEmptyBorder(kButtonContainerInsets))
              .AddChildren(
                  views::Builder<views::MdTextButton>()
                      .SetText(GetDeclineButtonText())
                      .SetID(magic_boost::ViewId::DisclaimerViewDeclineButton)
                      .SetAccessibleName(GetDeclineButtonText())
                      // Sets the button's height to a customized
                      // `kButtonHeight` instead of using the default
                      // height.
                      .SetMaxSize(gfx::Size(kImageWidth, kButtonHeight))
                      .SetStyle(ui::ButtonStyle::kProminent)
                      .SetCallback(std::move(press_decline_button_callback)),
                  views::Builder<views::MdTextButton>()
                      .CopyAddressTo(&accept_button_)
                      .SetID(magic_boost::ViewId::DisclaimerViewAcceptButton)
                      .SetText(GetAcceptButtonText())
                      .SetAccessibleName(GetAcceptButtonText())
                      .SetMaxSize(gfx::Size(kImageWidth, kButtonHeight))
                      .SetStyle(ui::ButtonStyle::kProminent)
                      .SetCallback(std::move(press_accept_button_callback))))
      .BuildChildren();

  disclaimer_view->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kRadius});
}

MagicBoostDisclaimerView::~MagicBoostDisclaimerView() = default;

// static
views::UniqueWidgetPtr MagicBoostDisclaimerView::CreateWidget(
    int64_t display_id,
    base::RepeatingClosure press_accept_button_callback,
    base::RepeatingClosure press_decline_button_callback) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.corner_radius = kRadius;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = GetWidgetName();

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<MagicBoostDisclaimerView>(
      std::move(press_accept_button_callback),
      std::move(press_decline_button_callback)));

  // Shows the widget in the middle of the screen.
  aura::Window* window = Shell::GetRootWindowForDisplayId(display_id);

  if (!window) {
    window = Shell::GetPrimaryRootWindow();
  }
  auto center = window->bounds().CenterPoint();
  widget->SetBounds(gfx::Rect(center.x() - kWidgetWidth / 2,
                              center.y() - kWidgetHeight / 2, kWidgetWidth,
                              kWidgetHeight));

  return widget;
}

// static
const char* MagicBoostDisclaimerView::GetWidgetName() {
  return kWidgetName;
}

void MagicBoostDisclaimerView::RequestFocus() {
  views::View::RequestFocus();

  accept_button_->RequestFocus();
}

BEGIN_METADATA(MagicBoostDisclaimerView)
END_METADATA

}  // namespace ash
