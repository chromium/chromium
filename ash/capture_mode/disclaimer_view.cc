// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/disclaimer_view.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
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
#include "ui/gfx/geometry/rounded_corners_f.h"
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
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// Paddings, sizes and insets.
constexpr int kBetweenButtonsSpacing = 8;
constexpr int kButtonHeight = 32;
constexpr int kContainerBottomPadding = 28;
constexpr int kContainerPadding = 32;
constexpr int kImageHeight = 260;
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

std::u16string GetTextTitle(bool is_reminder) {
  return l10n_util::GetStringUTF16(
      is_reminder ? IDS_ASH_SCANNER_DISCLAIMER_REMINDER_TITLE
                  : IDS_ASH_SCANNER_DISCLAIMER_TITLE);
}

std::u16string GetTextAcceptButton(bool is_reminder) {
  return l10n_util::GetStringUTF16(
      is_reminder ? IDS_ASH_SCANNER_DISCLAIMER_REMINDER_ACCEPT
                  : IDS_ASH_SCANNER_DISCLAIMER_ACCEPT);
}

std::u16string GetTextDeclineButton() {
  return l10n_util::GetStringUTF16(IDS_ASH_SCANNER_DISCLAIMER_DECLINE);
}

views::Builder<views::StyledLabel> GetTextBodyBuilder() {
  // There are various issues with using `ash::TypographyToken::kCrosBody1`:
  //
  // - It is using Google Sans, not Google Sans Text, which is not suitable for
  //   body text. See b/256663656 for more details.
  // - The only way of using it with `views::StyledLabel` is to use a
  //   `StyledLabel::RangeStyleInfo` with a `custom_font`. Setting this font for
  //   the entire label requires re-specifying the `custom_font` for _all_
  //   `StyledLabel::RangeStyleInfo`s.
  // - …excluding links, which cannot have a `custom_font`:
  //   https://crsrc.org/s?q=%22CHECK(!style_info.custom_font)%22
  //   Links need to use the default system font of `IDS_UI_FONT_FAMILY_CROS`,
  //   which is Roboto.
  //
  // Use a text style of `views::style::TextStyle::STYLE_BODY_2` (14pt with 20pt
  // line height) instead, which matches `kCrosBody1` but uses Roboto instead of
  // Google Sans. This also keeps the font consistent with links.
  //
  // TODO: b/256663656 - Use Google Sans Text here, preferably by updating
  // `IDS_UI_FONT_FAMILY_CROS`.
  //
  // Using a `views::style::TextContext` of `CONTEXT_DIALOG_BODY_TEXT` with the
  // aforementioned text style will result in a `ui::ColorId` of
  // `kColorLabelForeground`:
  // https://crsrc.org/s?q=s:TypographyProvider::GetColorIdImpl%20f:%5Eui.*cc$
  // This is the same as `kColorPrimaryForeground`:
  // https://crsrc.org/s?q=%22mixer%5BkColorLabelForeground%5D%22
  // which is the same as `kColorSysOnSurface`:
  // https://crsrc.org/s?q=%22mixer%5BkColorPrimaryForeground%5D%22%20f:material
  return views::Builder<views::StyledLabel>()
      .SetDefaultTextStyle(views::style::TextStyle::STYLE_BODY_2)
      .SetTextContext(views::style::TextContext::CONTEXT_DIALOG_BODY_TEXT)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
}

views::StyledLabel::RangeStyleInfo GetLinkTextStyleInfo(
    base::RepeatingClosure callback) {
  auto info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(std::move(callback));
  // This results in a `ui::ColorId` of `kColorLinkForeground`:
  // https://crsrc.org/s?q=s:TypographyProvider::GetColorIdImpl%20f:%5Eui.*cc$
  // which is `kColorSysPrimary`:
  // https://crsrc.org/s?q=%22mixer%5BkColorLinkForeground%5D%22%20f:material
  info.text_style = views::style::STYLE_LINK_2;
  return info;
}

views::Builder<views::StyledLabel> GetParagraphOneBuilder(
    base::RepeatingClosure press_terms_of_service_callback) {
  std::vector<size_t> offsets;
  std::u16string link_text =
      l10n_util::GetStringUTF16(IDS_ASH_SCANNER_DISCLAIMER_TERMS_LINK_TEXT);
  std::u16string text = l10n_util::GetStringFUTF16(
      IDS_ASH_SCANNER_DISCLAIMER_PARAGRAPH_ONE, {link_text}, &offsets);
  CHECK_EQ(offsets.size(), 1u);

  return GetTextBodyBuilder()
      .SetText(std::move(text))
      .AddStyleRange(
          gfx::Range(offsets[0], offsets[0] + link_text.size()),
          GetLinkTextStyleInfo(std::move(press_terms_of_service_callback)))
      .SetID(DisclaimerViewId::kDisclaimerViewParagraphOneId);
}

views::Builder<views::StyledLabel> GetParagraphTwoBuilder() {
  std::u16string text =
      l10n_util::GetStringUTF16(IDS_ASH_SCANNER_DISCLAIMER_PARAGRAPH_TWO);
  return GetTextBodyBuilder()
      .SetText(std::move(text))
      .SetID(DisclaimerViewId::kDisclaimerViewParagraphTwoId);
}

views::Builder<views::StyledLabel> GetParagraphThreeBuilder(
    base::RepeatingClosure press_learn_more_link_callback) {
  std::vector<size_t> offsets;
  std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_ASH_SCANNER_DISCLAIMER_LEARN_MORE_LINK_TEXT);
  std::u16string text = l10n_util::GetStringFUTF16(
      IDS_ASH_SCANNER_DISCLAIMER_PARAGRAPH_THREE, {link_text}, &offsets);
  CHECK_EQ(offsets.size(), 1u);

  return GetTextBodyBuilder()
      .SetText(std::move(text))
      .AddStyleRange(
          gfx::Range(offsets[0], offsets[0] + link_text.size()),
          GetLinkTextStyleInfo(std::move(press_learn_more_link_callback)))
      .SetID(DisclaimerViewId::kDisclaimerViewParagraphThreeId);
}

ui::ImageModel GetDisclaimerIllustration() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return ui::ImageModel::FromResourceId(
      IDR_SCANNER_DISCLAIMER_ILLUSTRATION_PNG);
#else
  return ui::ImageModel();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace

DisclaimerView::DisclaimerView(
    bool is_reminder,
    base::RepeatingClosure press_accept_button_callback,
    base::RepeatingClosure press_decline_button_callback,
    base::RepeatingClosure press_terms_of_service_callback,
    base::RepeatingClosure press_learn_more_link_callback) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysDialogContainer, kRadius));
  SetPaintToLayer();
  AddChildView(views::Builder<views::ImageView>()
                   .SetImage(GetDisclaimerIllustration())
                   .SetImageSize(kImagePreferredSize)
                   .SetPreferredSize(kImagePreferredSize)
                   .SetBackground(views::CreateSolidBackground(
                       cros_tokens::kCrosSysIlloColor12))
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
                  .SetInsideBorderInsets(kTextContainerInsets)
                  .AddChildren(
                      views::Builder<views::Label>()
                          .SetFontList(
                              TypographyProvider::Get()->ResolveTypographyToken(
                                  TypographyToken::kCrosDisplay7))
                          .SetEnabledColor(cros_tokens::kCrosSysOnSurface)
                          .SetHorizontalAlignment(
                              gfx::HorizontalAlignment::ALIGN_LEFT)
                          .SetText(GetTextTitle(is_reminder))
                          .SetAccessibleRole(ax::mojom::Role::kHeading)
                          .CopyAddressTo(&title_),
                      GetParagraphOneBuilder(
                          std::move(press_terms_of_service_callback)),
                      GetParagraphTwoBuilder(),
                      GetParagraphThreeBuilder(
                          std::move(press_learn_more_link_callback))))
          .Build());

  auto button_layout_builder =
      views::Builder<views::BoxLayoutView>()
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetBetweenChildSpacing(kBetweenButtonsSpacing)
          .SetInsideBorderInsets(kButtonContainerInsets);
  if (!is_reminder) {
    button_layout_builder.AddChild(
        views::Builder<views::MdTextButton>()
            .SetText(GetTextDeclineButton())
            .SetAccessibleName(GetTextDeclineButton())
            .SetMaxSize(gfx::Size(kImageWidth, kButtonHeight))
            .SetStyle(ui::ButtonStyle::kProminent)
            .SetCallback(std::move(press_decline_button_callback))
            .SetID(kDisclaimerViewDeclineButtonId));
  }
  button_layout_builder.AddChild(
      views::Builder<views::MdTextButton>()
          .SetText(GetTextAcceptButton(is_reminder))
          .SetAccessibleName(GetTextAcceptButton(is_reminder))
          .SetMaxSize(gfx::Size(kImageWidth, kButtonHeight))
          .SetStyle(ui::ButtonStyle::kProminent)
          .SetCallback(std::move(press_accept_button_callback))
          .CopyAddressTo(&accept_button_)
          .SetID(kDisclaimerViewAcceptButtonId));

  AddChildView(std::move(button_layout_builder).Build());

  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kRadius});
}

DisclaimerView::~DisclaimerView() = default;

// static
std::unique_ptr<views::Widget> DisclaimerView::CreateWidget(
    aura::Window* const root,
    bool is_reminder,
    base::RepeatingClosure press_accept_button_callback,
    base::RepeatingClosure press_decline_button_callback,
    base::RepeatingClosure press_terms_of_service_callback,
    base::RepeatingClosure press_learn_more_link_callback) {
  auto disclaimer_view = std::make_unique<DisclaimerView>(
      is_reminder, std::move(press_accept_button_callback),
      std::move(press_decline_button_callback),
      std::move(press_terms_of_service_callback),
      std::move(press_learn_more_link_callback));

  auto delegate = std::make_unique<views::WidgetDelegate>();
  delegate->SetOwnedByWidget(views::WidgetDelegate::OwnedByWidgetPassKey());
  delegate->SetAccessibleWindowRole(ax::mojom::Role::kDialog);
  delegate->SetAccessibleTitle(GetTextTitle(is_reminder));
  delegate->SetInitiallyFocusedView(disclaimer_view->accept_button());

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  const gfx::Rect work_area(
      display::Screen::Get()->GetDisplayNearestWindow(root).work_area());
  params.delegate = delegate.release();
  params.parent =
      Shell::GetContainer(root, kShellWindowId_CaptureModeSearchResultsPanel);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.rounded_corners = gfx::RoundedCornersF(kRadius);
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
