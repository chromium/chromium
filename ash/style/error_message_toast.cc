// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/error_message_toast.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kErrorMessageViewSize = 34;
constexpr int kErrorMessageRoundedCornerRadius = kErrorMessageViewSize / 2;
constexpr gfx::Insets kButtonInsets = gfx::Insets::TLBR(8, 4, 8, 10);
constexpr gfx::Insets kLabelInsets = gfx::Insets::TLBR(0, 16, 0, 0);

class ActionLabelButton : public views::LabelButton {
  METADATA_HEADER(ActionLabelButton, views::LabelButton)

 public:
  ActionLabelButton(PressedCallback callback,
                    ErrorMessageToast::ButtonActionType type)
      : views::LabelButton(std::move(callback)) {
    int string_id;
    switch (type) {
      case ErrorMessageToast::ButtonActionType::kDismiss:
        string_id = IDS_ASH_ERROR_MESSAGE_TOAST_DISMISS;
        break;
      case ErrorMessageToast::ButtonActionType::kReload:
        string_id = IDS_ASH_ERROR_MESSAGE_TOAST_RELOAD;
        break;
    }
    SetText(l10n_util::GetStringUTF16(string_id));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT);
    SetProperty(views::kMarginsKey, kButtonInsets);
    SetEnabledTextColorIds(cros_tokens::kCrosSysPrimary);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label());
    label()->SetAutoColorReadabilityEnabled(false);
  }
  ~ActionLabelButton() override = default;
};

BEGIN_METADATA(ActionLabelButton)
END_METADATA

}  // namespace

ErrorMessageToast::ErrorMessageToast(views::Button::PressedCallback callback,
                                     const std::u16string& error_message,
                                     ButtonActionType type,
                                     ui::ColorId background_color_id) {
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kErrorMessageRoundedCornerRadius));
  SetBackground(views::CreateThemedSolidBackground(background_color_id));

  const auto* const typography_provider = TypographyProvider::Get();
  error_message_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetFontList(typography_provider->ResolveTypographyToken(
              TypographyToken::kCrosAnnotation1))
          .SetLineHeight(typography_provider->ResolveLineHeight(
              TypographyToken::kCrosAnnotation1))
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetText(error_message)
          .SetProperty(views::kMarginsKey, kLabelInsets)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded))
          .Build());
  error_message_label_->SetAutoColorReadabilityEnabled(false);

  action_button_ = AddChildView(
      std::make_unique<ActionLabelButton>(std::move(callback), type));
}

void ErrorMessageToast::UpdateBoundsToContainer(
    const gfx::Rect& container_bounds,
    const gfx::Insets& padding) {
  gfx::Rect preferred_bounds(container_bounds);

  preferred_bounds.Inset(gfx::Insets::TLBR(
      preferred_bounds.height() - kErrorMessageViewSize - padding.bottom(),
      padding.left(), padding.bottom(), padding.right()));

  SetBoundsRect(preferred_bounds);
}

std::u16string ErrorMessageToast::GetMessageForTest() const {
  return error_message_label_->GetText();
}

BEGIN_METADATA(ErrorMessageToast)
END_METADATA

}  // namespace ash
