// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/native_theme/native_theme_aura.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace policy {

namespace {

// The corner radius of the bubble.
constexpr int kBubbleCornerRadius = 8;
constexpr gfx::RoundedCornersF kCornerRadii(kBubbleCornerRadius);

// The blur radius for the bubble background.
constexpr int kBubbleBlurRadius = 80;

// The size of the managed icon.
constexpr int kManagedIconSize = 20;

// The maximum width of the bubble.
constexpr int kBubbleWidth = 360;

// The spacing between the icon and label in the bubble.
constexpr int kIconLabelSpacing = 16;

// The padding which separates the bubble border with its inner contents.
constexpr int kBubblePadding = 16;

// The line height of the bubble text.
constexpr int kLineHeight = 20;

// The insets of the bubble borders.
constexpr gfx::Insets kBubbleBorderInsets(1);

// The font name of the text used in the bubble.
constexpr char kTextFontName[] = "Roboto";

// The font size of the text used in the bubble.
constexpr int kTextFontSize = 13;

// The height of the dismiss button.
constexpr int kButtonHeight = 32;

// The padding which separates the button border with its inner contents.
constexpr int kButtonPadding = 16;

// The spacing between the button border and label.
constexpr int kButtonLabelSpacing = 8;

// The spacing between the buttons.
constexpr int kButtonsSpacing = 8;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/1311180) Replace color retrieval with more long term solution.
SkColor RetrieveColor(cros_styles::ColorName name) {
  return cros_styles::ResolveColor(
      name, ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors(),
      /*use_debug_colors=*/false);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

class BubbleButton : public views::LabelButton {
  METADATA_HEADER(BubbleButton, views::LabelButton)

 public:
  explicit BubbleButton(const std::u16string& button_label) {
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

    SetText(button_label);

    const gfx::FontList font_list = GetFontList();
    label()->SetFontList(font_list);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    const SkColor text_color = ash::ColorProvider::Get()->GetContentLayerColor(
        ash::ColorProvider::ContentLayerType::kButtonLabelColorBlue);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(crbug.com/1311180) Replace color retrieval with more long term
    // solution.
    const SkColor text_color =
        ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
            ? gfx::kGoogleBlue300
            : gfx::kGoogleBlue600;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    SetTextColor(ButtonState::STATE_NORMAL, text_color);
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetSize({gfx::GetStringWidth(button_label, font_list) + 2 * kButtonPadding,
             kButtonHeight});
  }

  BubbleButton(const BubbleButton&) = delete;
  BubbleButton& operator=(const BubbleButton&) = delete;
  ~BubbleButton() override = default;

  int GetLabelWidth() const { return label()->bounds().width(); }

  static gfx::FontList GetFontList() {
    return gfx::FontList({kTextFontName}, gfx::Font::NORMAL, kTextFontSize,
                         gfx::Font::Weight::MEDIUM);
  }
};

void OnLearnMoreLinkClicked() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(dlp::kDlpLearnMoreUrl),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // The dlp policy applies to the main profile, so use the main profile for
  // opening the page.
  NavigateParams navigate_params(
      ProfileManager::GetPrimaryUserProfile(), GURL(dlp::kDlpLearnMoreUrl),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&navigate_params);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

BEGIN_METADATA(BubbleButton)
ADD_READONLY_PROPERTY_METADATA(int, LabelWidth)
END_METADATA

ClipboardBubbleView::ClipboardBubbleView(const std::u16string& text) {
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1311180) Replace color retrieval with more long term
  // solution.
  layer()->SetColor(RetrieveColor(cros_styles::ColorName::kBgColor));
  layer()->SetBackgroundBlur(kBubbleBlurRadius);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  layer()->SetBackgroundBlur(kBubbleBlurRadius);
  layer()->SetRoundedCornerRadius(kCornerRadii);

  // Add the managed icon.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
  const SkColor icon_color = color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kIconColorPrimary);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1311180) Replace color retrieval with more long term
  // solution.
  const SkColor icon_color =
      RetrieveColor(cros_styles::ColorName::kIconColorPrimary);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  managed_icon_ = AddChildView(std::make_unique<views::ImageView>());
  managed_icon_->SetPaintToLayer();
  managed_icon_->layer()->SetFillsBoundsOpaquely(false);
  managed_icon_->SetBounds(kBubblePadding, kBubblePadding, kManagedIconSize,
                           kManagedIconSize);
  managed_icon_->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                                kManagedIconSize, icon_color));

  // Add the bubble text.
  label_ = AddChildView(std::make_unique<views::StyledLabel>());
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->SetPosition(gfx::Point(
      kBubblePadding + kManagedIconSize + kIconLabelSpacing, kBubblePadding));
  label_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);

  std::u16string learn_more_link_text =
      l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  std::u16string full_text = l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BUBBLE_MESSAGE, text, learn_more_link_text);
  const int main_message_length =
      full_text.size() - learn_more_link_text.size();

  // Set the styling of the main text.
  // TODO(crbug.com/1150741): Handle RTL.
  views::StyledLabel::RangeStyleInfo message_style;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  message_style.override_color = color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorPrimary);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1311180) Replace color retrieval with more long term
  // solution.
  message_style.override_color =
      RetrieveColor(cros_styles::ColorName::kTextColorPrimary);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  label_->SetText(full_text);
  label_->AddStyleRange(gfx::Range(0, main_message_length), message_style);

  // Add "Learn more" link.
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&OnLearnMoreLinkClicked));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  link_style.override_color = color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorURL);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1311180) Replace color retrieval with more long term
  // solution.
  link_style.override_color =
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
          ? gfx::kGoogleBlue300
          : gfx::kGoogleBlue600;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  label_->AddStyleRange(gfx::Range(main_message_length, full_text.size()),
                        link_style);
  label_->SetLineHeight(kLineHeight);
  label_->SizeToFit(kBubbleWidth - 2 * kBubblePadding - kManagedIconSize -
                    kIconLabelSpacing);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Bubble borders
  border_ = AddChildView(std::make_unique<views::ImageView>());
  border_->SetPaintToLayer();
  border_->layer()->SetFillsBoundsOpaquely(false);
  auto shadow_border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::FLOAT, views::BubbleBorder::STANDARD_SHADOW);
  shadow_border->SetCornerRadius(kBubbleCornerRadius);
  shadow_border->SetColor(SK_ColorTRANSPARENT);
  shadow_border->set_insets(kBubbleBorderInsets);
  border_->SetSize({kBubbleWidth, INT_MAX});
  border_->SetBorder(std::move(shadow_border));
  border_->SetCanProcessEventsWithinSubtree(false);
}

ClipboardBubbleView::~ClipboardBubbleView() = default;

void ClipboardBubbleView::OnThemeChanged() {
  views::View::OnThemeChanged();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const SkColor background_color =
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemBaseElevated);
  layer()->SetColor(background_color);
  label_->SetDisplayedOnBackgroundColor(background_color);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ClipboardBubbleView::UpdateBorderSize(const gfx::Size& size) {
  border_->SetSize(size);
}

BEGIN_METADATA(ClipboardBubbleView)
ADD_READONLY_PROPERTY_METADATA(gfx::Size, BubbleSize)
END_METADATA

ClipboardBlockBubble::ClipboardBlockBubble(const std::u16string& text)
    : ClipboardBubbleView(text) {
  // Add "Got it" button.
  std::u16string button_label =
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_CLIPBOARD_BLOCK_DISMISS_BUTTON);
  button_ = AddChildView(std::make_unique<BubbleButton>(button_label));
  button_->SetPaintToLayer();
  button_->layer()->SetFillsBoundsOpaquely(false);
  button_->SetPosition(
      gfx::Point(kBubbleWidth - kBubblePadding - button_->width(),
                 kBubblePadding + label_->height() + kButtonLabelSpacing));

  UpdateBorderSize(GetBubbleSize());
}

ClipboardBlockBubble::~ClipboardBlockBubble() = default;

gfx::Size ClipboardBlockBubble::GetBubbleSize() const {
  DCHECK(label_);
  DCHECK(button_);
  return {kBubbleWidth, 2 * kBubblePadding + label_->bounds().height() +
                            kButtonLabelSpacing + button_->height()};
}

void ClipboardBlockBubble::SetDismissCallback(base::OnceClosure cb) {
  DCHECK(button_);
  button_->SetCallback(std::move(cb));
}

BEGIN_METADATA(ClipboardBlockBubble)
END_METADATA

ClipboardWarnBubble::ClipboardWarnBubble(const std::u16string& text)
    : ClipboardBubbleView(text) {
  // Add paste button.
  std::u16string paste_label =
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_CLIPBOARD_WARN_PROCEED_BUTTON);
  paste_button_ = AddChildView(std::make_unique<BubbleButton>(paste_label));
  paste_button_->SetPaintToLayer();
  paste_button_->layer()->SetFillsBoundsOpaquely(false);
  paste_button_->SetPosition(
      gfx::Point(kBubbleWidth - kBubblePadding - paste_button_->width(),
                 kBubblePadding + label_->height() + kButtonLabelSpacing));

  // Add cancel button.
  std::u16string cancel_label =
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON);
  cancel_button_ = AddChildView(std::make_unique<BubbleButton>(cancel_label));
  cancel_button_->SetPaintToLayer();
  cancel_button_->layer()->SetFillsBoundsOpaquely(false);
  cancel_button_->SetPosition(
      gfx::Point(kBubbleWidth - kBubblePadding - paste_button_->width() -
                     kButtonsSpacing - cancel_button_->width(),
                 kBubblePadding + label_->height() + kButtonLabelSpacing));

  UpdateBorderSize(GetBubbleSize());
}

ClipboardWarnBubble::~ClipboardWarnBubble() {
  if (paste_cb_) {
    std::move(paste_cb_).Run(false);
  }
}

gfx::Size ClipboardWarnBubble::GetBubbleSize() const {
  DCHECK(label_);
  DCHECK(cancel_button_);
  DCHECK(paste_button_);
  return {kBubbleWidth, 2 * kBubblePadding + label_->bounds().height() +
                            kButtonLabelSpacing + paste_button_->height()};
}

void ClipboardWarnBubble::SetDismissCallback(base::OnceClosure cb) {
  DCHECK(cancel_button_);
  cancel_button_->SetCallback(std::move(cb));
}

void ClipboardWarnBubble::SetProceedCallback(base::OnceClosure cb) {
  DCHECK(paste_button_);
  paste_button_->SetCallback(std::move(cb));
}

BEGIN_METADATA(ClipboardWarnBubble)
END_METADATA

}  // namespace policy
