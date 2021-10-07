// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_input_container.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {

// Padding between children, currently only between the textfield and the
// ImageButton.
constexpr int kBetweenChildSpacing = 12;

// Insets for inside the border.
constexpr gfx::Insets kInsideBorderInsets(6, 16, 14, 16);

// The icon size of inline reply input field.
constexpr int kInputReplyButtonSize = 20;
// Padding on the input reply button.
constexpr gfx::Insets kInputReplyButtonPadding(0, 6, 0, 6);
// Radius of the circular input reply button highlight.
constexpr int kInputReplyHighlightRadius =
    (kInputReplyButtonPadding.width() + kInputReplyButtonSize) / 2;

// Padding of the textfield, inside the rounded background.
constexpr gfx::Insets kInputTextfieldPaddingCrOS(6, 12, 6, 12);
// Corner radius of the grey background of the textfield.
constexpr int kTextfieldBackgroundCornerRadius = 24;

// Inline reply textfield's highlight path generator. Draws a highlight path
// that is flush with the rounded background.
class TextfieldHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit TextfieldHighlightPathGenerator(views::Textfield* textfield)
      : textfield_(textfield) {}
  TextfieldHighlightPathGenerator(const TextfieldHighlightPathGenerator&) =
      delete;
  TextfieldHighlightPathGenerator& operator=(
      const TextfieldHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  absl::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    return gfx::RRectF(gfx::RectF(textfield_->GetLocalBounds()),
                       kTextfieldBackgroundCornerRadius);
  }

 private:
  // Owned by the view hierarchy.
  const views::Textfield* const textfield_;
};

// Inline reply's send button's highlight path generator. Draws a circular
// highlight path.
class SendButtonHighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit SendButtonHighlightPathGenerator(views::ImageButton* button)
      : button_(button) {}
  SendButtonHighlightPathGenerator(const SendButtonHighlightPathGenerator&) =
      delete;
  SendButtonHighlightPathGenerator& operator=(
      const SendButtonHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  absl::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    return gfx::RRectF(gfx::RectF(button_->GetLocalBounds()),
                       kInputReplyHighlightRadius);
  }

 private:
  // Owned by the views hierarchy.
  const views::ImageButton* const button_;
};

}  // namespace

AshNotificationInputContainer::AshNotificationInputContainer(
    message_center::NotificationInputDelegate* delegate)
    : message_center::NotificationInputContainer(delegate) {}

AshNotificationInputContainer::~AshNotificationInputContainer() {}

views::BoxLayout* AshNotificationInputContainer::InstallLayoutManager() {
  return SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kInsideBorderInsets,
      kBetweenChildSpacing));
}

views::InkDropContainerView* AshNotificationInputContainer::InstallInkDrop() {
  // Do not install an inkdrop.
  return nullptr;
}

gfx::Insets AshNotificationInputContainer::GetTextfieldPadding() const {
  return kInputTextfieldPaddingCrOS;
}

int AshNotificationInputContainer::GetDefaultPlaceholderStringId() const {
  return IDS_ASH_NOTIFICATION_INLINE_REPLY_PLACEHOLDER;
}

void AshNotificationInputContainer::StyleTextfield() {
  textfield()->SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                         kNotificationBodyFontWeight,
                                         gfx::Font::Weight::MEDIUM));
  auto* color_provider = ash::AshColorProvider::Get();
  textfield()->SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetControlsLayerColor(
          ash::AshColorProvider::ControlsLayerType::
              kControlBackgroundColorInactive),
      kTextfieldBackgroundCornerRadius));

  views::FocusRing::Install(textfield());
  views::HighlightPathGenerator::Install(
      textfield(),
      std::make_unique<TextfieldHighlightPathGenerator>(textfield()));
}

gfx::Insets AshNotificationInputContainer::GetSendButtonPadding() const {
  return kInputReplyButtonPadding;
}

void AshNotificationInputContainer::SetSendButtonHighlightPath() {
  views::HighlightPathGenerator::Install(
      button(), std::make_unique<SendButtonHighlightPathGenerator>(button()));
}

void AshNotificationInputContainer::UpdateButtonImage() {
  if (!GetWidget())
    return;

  button()->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kSendIcon, kInputReplyButtonSize,
                            ash::AshColorProvider::Get()->GetControlsLayerColor(
                                ash::AshColorProvider::ControlsLayerType::
                                    kControlBackgroundColorActive)));
}

}  // namespace ash
