// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/magic_boost/magic_boost_constants.h"
#include "base/functional/bind.h"
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
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr char kWidgetName[] = "MagicBoostDisclaimerViewWidget";

// Paddings, sizes and insets.
constexpr int kImageWidth = 512;
constexpr int kImageHeight = 236;
constexpr int kContainerPadding = 32;
constexpr int kTextContainerBetweenChildSpacing = 16;
constexpr int kContainerBottomPadding = 28;
constexpr int kWidgetWidth = kImageWidth;
constexpr int kWidgetMiniumHeight = 650;
constexpr int kBetweenButtonsSpacing = 8;
constexpr int kButtonHeight = 32;
constexpr int kRadius = 20;

constexpr gfx::Insets kButtonContainerInsets =
    gfx::Insets::TLBR(0,
                      kContainerPadding,
                      kContainerBottomPadding,
                      kContainerPadding);
constexpr gfx::Insets kTextContainerInsets = gfx::Insets(kContainerPadding);
constexpr gfx::Size kImagePreferredSize(/*width=*/kImageWidth,
                                        /*height=*/kImageHeight);

views::StyledLabel::RangeStyleInfo GetBodyTextStyle() {
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosBody1);
  style.override_color_id = static_cast<ui::ColorId>(ui::kColorSysOnSurface);
  return style;
}

views::StyledLabel::RangeStyleInfo GetLinkTextStyle(
    base::RepeatingClosure press_link_callback) {
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          std::move(press_link_callback));
  link_style.override_color_id =
      static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurfaceVariant);
  return link_style;
}

views::Builder<views::StyledLabel> GetTextBodyBuilder(
    const std::u16string& text) {
  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .AddStyleRange(gfx::Range(0, text.length()), GetBodyTextStyle())
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
}

views::Builder<views::StyledLabel> GetParagraphOneBuilder() {
  return GetTextBodyBuilder(l10n_util::GetStringUTF16(
                                IDS_ASH_MAGIC_BOOST_DISCLAMIER_PARAGRAPH_ONE))
      .SetID(magic_boost::ViewId::DisclaimerViewParagraphOne);
}

views::Builder<views::StyledLabel> GetParagraphTwoBuilder(
    base::RepeatingClosure press_link_callback) {
  std::vector<size_t> offsets;
  const std::u16string link_text =
      l10n_util::GetStringUTF16(IDS_ASH_MAGIC_BOOST_DISCLAIMER_TERMS_LINK_TEXT);
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_ASH_MAGIC_BOOST_DISCLAIMER_PARAGRAPH_TWO, {link_text}, &offsets);

  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .SetID(magic_boost::ViewId::DisclaimerViewParagraphTwo)
      .AddStyleRange(gfx::Range(0, offsets.at(0)), GetBodyTextStyle())
      .AddStyleRange(
          gfx::Range(offsets.at(0), offsets.at(0) + link_text.length()),
          GetLinkTextStyle(std::move(press_link_callback)))
      .AddStyleRange(
          gfx::Range(offsets.at(0) + link_text.length(), text.length()),
          GetBodyTextStyle())
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
}

views::Builder<views::StyledLabel> GetParagraphThreeBuilder() {
  return GetTextBodyBuilder(l10n_util::GetStringUTF16(
                                IDS_ASH_MAGIC_BOOST_DISCLAIMER_PARAGRAPH_THREE))
      .SetID(magic_boost::ViewId::DisclaimerViewParagraphThree);
}

views::Builder<views::StyledLabel> GetParagraphFourBuilder(
    base::RepeatingClosure press_link_callback) {
  std::vector<size_t> offsets;
  const std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_ASH_MAGIC_BOOST_DISCLAIMER_LEARN_MORE_LINK_TEXT);
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_ASH_MAGIC_BOOST_DISCLAIMER_PARAGRAPH_FOUR, {link_text}, &offsets);

  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .SetID(magic_boost::ViewId::DisclaimerViewParagraphFour)
      .AddStyleRange(gfx::Range(0, offsets.at(0)), GetBodyTextStyle())
      .AddStyleRange(
          gfx::Range(offsets.at(0), offsets.at(0) + link_text.length()),
          GetLinkTextStyle(std::move(press_link_callback)))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
}

}  // namespace

MagicBoostDisclaimerView::MagicBoostDisclaimerView(
    base::RepeatingClosure press_accept_button_callback,
    base::RepeatingClosure press_decline_button_callback,
    base::RepeatingClosure press_terms_of_service_callback,
    base::RepeatingClosure press_learn_more_link_callback) {
  views::View* disclaimer_view;
  std::u16string decline_button_text =
      l10n_util::GetStringUTF16(IDS_ASH_MAGIC_BOOST_DISCLAIMER_DECLINE_BUTTON);
  std::u16string accept_button_text =
      l10n_util::GetStringUTF16(IDS_ASH_MAGIC_BOOST_DISCLAIMER_ACCEPT_BUTTON);
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
                                  TypographyProvider::Get()
                                      ->ResolveTypographyToken(
                                          TypographyToken::kCrosDisplay7))
                              .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                              .SetHorizontalAlignment(
                                  gfx::HorizontalAlignment::ALIGN_LEFT)
                              .SetID(magic_boost::ViewId::DisclaimerViewTitle)
                              .CopyAddressTo(&title_)
                              .SetText(l10n_util::GetStringUTF16(
                                  IDS_ASH_MAGIC_BOOST_DISCLAIMER_TITLE)),
                          GetParagraphOneBuilder().CopyAddressTo(
                              &paragraph_one_),
                          GetParagraphTwoBuilder(
                              std::move(press_terms_of_service_callback))
                              .CopyAddressTo(&paragraph_two_),
                          GetParagraphThreeBuilder().CopyAddressTo(
                              &paragraph_three_),
                          GetParagraphFourBuilder(
                              std::move(press_learn_more_link_callback))
                              .CopyAddressTo(&paragraph_four_))),
          views::Builder<views::BoxLayoutView>()
              .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
              .SetBetweenChildSpacing(kBetweenButtonsSpacing)
              .SetBorder(views::CreateEmptyBorder(kButtonContainerInsets))
              .AddChildren(
                  views::Builder<views::MdTextButton>()
                      .SetID(magic_boost::ViewId::DisclaimerViewDeclineButton)
                      .SetText(decline_button_text)
                      .SetAccessibleName(decline_button_text)
                      // Sets the button's height to a customized
                      // `kButtonHeight` instead of using the default
                      // height.
                      .SetMaxSize(gfx::Size(kImageWidth, kButtonHeight))
                      .SetStyle(ui::ButtonStyle::kProminent)
                      .SetCallback(std::move(press_decline_button_callback)),
                  views::Builder<views::MdTextButton>()
                      .CopyAddressTo(&accept_button_)
                      .SetID(magic_boost::ViewId::DisclaimerViewAcceptButton)
                      .SetText(accept_button_text)
                      .SetAccessibleName(accept_button_text)
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
    base::RepeatingClosure press_decline_button_callback,
    base::RepeatingClosure press_disclaimer_link_callback,
    base::RepeatingClosure press_learn_more_link_callback) {
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
  auto disclaimer_view = std::make_unique<MagicBoostDisclaimerView>(
      std::move(press_accept_button_callback),
      std::move(press_decline_button_callback),
      std::move(press_disclaimer_link_callback),
      std::move(press_learn_more_link_callback));
  const int widget_height = disclaimer_view->GetPreferredSize().height();
  widget->SetContentsView(std::move(disclaimer_view));

  // Shows the widget in the middle of the screen.
  aura::Window* window = Shell::GetRootWindowForDisplayId(display_id);

  if (!window) {
    window = Shell::GetPrimaryRootWindow();
  }

  auto center = window->GetBoundsInScreen().CenterPoint();
  widget->SetBounds(gfx::Rect(center.x() - kWidgetWidth / 2,
                              center.y() - widget_height / 2, kWidgetWidth,
                              widget_height));

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

gfx::Size MagicBoostDisclaimerView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int body_width = kWidgetWidth - 2 * kContainerPadding;
  int height = kImageHeight + paragraph_one_->GetHeightForWidth(body_width) +
               paragraph_two_->GetHeightForWidth(body_width) +
               paragraph_three_->GetHeightForWidth(body_width) +
               paragraph_four_->GetHeightForWidth(body_width) +
               title_->GetHeightForWidth(body_width) +
               accept_button_->GetHeightForWidth(body_width) +
               kContainerPadding * 2 + kContainerBottomPadding +
               kTextContainerBetweenChildSpacing * 4;
  height = std::max(height, kWidgetMiniumHeight);

  return gfx::Size(kWidgetWidth, height);
}

BEGIN_METADATA(MagicBoostDisclaimerView)
END_METADATA

}  // namespace ash
