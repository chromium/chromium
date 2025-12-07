// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/functional/bind.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"

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

class BubbleButton : public views::LabelButton {
  METADATA_HEADER(BubbleButton, views::LabelButton)

 public:
  explicit BubbleButton(const std::u16string& button_label) {
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

    SetText(button_label);

    const gfx::FontList font_list = GetFontList();
    label()->SetFontList(font_list);

    SetTextColor(ButtonState::STATE_NORMAL, cros_tokens::kTextColorProminent);
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
  ash::NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(dlp::kDlpLearnMoreUrl),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

}  // namespace

BEGIN_METADATA(BubbleButton)
ADD_READONLY_PROPERTY_METADATA(int, LabelWidth)
END_METADATA

ClipboardBubbleView::ClipboardBubbleView(const std::u16string& text) {
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  SetBackground(views::CreateLayerBasedRoundedBackground(
      chromeos::features::IsSystemBlurEnabled()
          ? cros_tokens::kCrosSysSystemBaseElevated
          : cros_tokens::kCrosSysSystemBaseElevatedOpaque,
      kCornerRadii));

  if (chromeos::features::IsSystemBlurEnabled()) {
    layer()->SetBackgroundBlur(kBubbleBlurRadius);
    layer()->SetBackdropFilterQuality(
        ash::ColorProvider::kBackgroundBlurQuality);
  }

  // Add the managed icon.
  managed_icon_ = AddChildView(std::make_unique<views::ImageView>());
  managed_icon_->SetPaintToLayer();
  managed_icon_->layer()->SetFillsBoundsOpaquely(false);
  managed_icon_->SetBounds(kBubblePadding, kBubblePadding, kManagedIconSize,
                           kManagedIconSize);
  managed_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, cros_tokens::kIconColorPrimary,
      kManagedIconSize));

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
  message_style.override_color_id = cros_tokens::kTextColorPrimary;

  label_->SetText(full_text);
  label_->AddStyleRange(gfx::Range(0, main_message_length), message_style);

  // Add "Learn more" link.
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&OnLearnMoreLinkClicked));
  link_style.override_color_id = cros_tokens::kLinkColor;

  label_->AddStyleRange(gfx::Range(main_message_length, full_text.size()),
                        link_style);
  label_->SetLineHeight(kLineHeight);
  label_->SizeToFit(kBubbleWidth - 2 * kBubblePadding - kManagedIconSize -
                    kIconLabelSpacing);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetDisplayedOnBackgroundColor(
      cros_tokens::kCrosSysSystemBaseElevated);

  // Bubble borders
  border_ = AddChildView(std::make_unique<views::ImageView>());
  border_->SetPaintToLayer();
  border_->layer()->SetFillsBoundsOpaquely(false);
  auto shadow_border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::FLOAT, views::BubbleBorder::STANDARD_SHADOW);
  shadow_border->set_rounded_corners(gfx::RoundedCornersF(kBubbleCornerRadius));
  shadow_border->SetColor(SK_ColorTRANSPARENT);
  shadow_border->set_insets(kBubbleBorderInsets);
  border_->SetSize({kBubbleWidth, INT_MAX});
  border_->SetBorder(std::move(shadow_border));
  border_->SetCanProcessEventsWithinSubtree(false);
}

ClipboardBubbleView::~ClipboardBubbleView() = default;

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
