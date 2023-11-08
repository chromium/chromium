// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_error_message_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kErrorMessageViewSize = 40;
constexpr int kErrorMessageHorizontalMargin = 16;
constexpr int kErrorMessageBottomMargin = 12;
constexpr gfx::Insets kButtonInsets = gfx::Insets::TLBR(10, 0, 10, 16);
constexpr gfx::Insets kLabelInsets = gfx::Insets::VH(10, 16);

}  // namespace

namespace ash {

class DismissErrorLabelButton : public views::LabelButton {
  METADATA_HEADER(DismissErrorLabelButton, views::LabelButton)

 public:
  explicit DismissErrorLabelButton(PressedCallback callback)
      : views::LabelButton(std::move(callback)) {
    SetText(l10n_util::GetStringUTF16(IDS_GLANCEABLES_ERROR_DISMISS));
    SetID(
        base::to_underlying(GlanceablesViewId::kGlanceablesErrorMessageButton));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT);
    SetProperty(views::kMarginsKey, kButtonInsets);
    SetTextColorId(views::Button::STATE_NORMAL, cros_tokens::kCrosSysOnError);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label());
    label()->SetAutoColorReadabilityEnabled(false);
  }
  ~DismissErrorLabelButton() override = default;
};

BEGIN_METADATA(DismissErrorLabelButton, views::LabelButton)
END_METADATA

GlanceablesErrorMessageView::GlanceablesErrorMessageView(
    views::Button::PressedCallback callback,
    const std::u16string& error_message) {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysError, kErrorMessageViewSize / 2));

  const auto* const typography_provider = TypographyProvider::Get();

  error_message_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetID(base::to_underlying(
              GlanceablesViewId::kGlanceablesErrorMessageLabel))
          .SetEnabledColorId(cros_tokens::kCrosSysOnError)
          .SetFontList(typography_provider->ResolveTypographyToken(
              TypographyToken::kCrosButton2))
          .SetLineHeight(typography_provider->ResolveLineHeight(
              TypographyToken::kCrosButton2))
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetText(error_message)
          .SetProperty(views::kMarginsKey, kLabelInsets)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded))
          .Build());
  error_message_label_->SetAutoColorReadabilityEnabled(false);

  dismiss_button_ = AddChildView(
      std::make_unique<DismissErrorLabelButton>(std::move(callback)));
}

void GlanceablesErrorMessageView::UpdateBoundsToContainer(
    const gfx::Rect& container_bounds) {
  gfx::Rect preferred_bounds(container_bounds);

  preferred_bounds.Inset(gfx::Insets::TLBR(
      preferred_bounds.height() - kErrorMessageViewSize -
          kErrorMessageBottomMargin,
      kErrorMessageHorizontalMargin, kErrorMessageBottomMargin,
      kErrorMessageHorizontalMargin));

  SetBoundsRect(preferred_bounds);
}

BEGIN_METADATA(GlanceablesErrorMessageView, views::View)
END_METADATA

}  // namespace ash
