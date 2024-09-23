// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/locked_tpm_message_view.h"

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr int kVerticalBorderDp = 16;
constexpr int kHorizontalBorderDp = 16;
constexpr int kChildrenSpacingDp = 4;
constexpr int kWidthDp = 360;
constexpr int kHeightDp = 108;
constexpr int kIconSizeDp = 24;
constexpr int kDeltaDp = 0;
constexpr int kRoundedCornerRadiusDp = 8;

}  // namespace

LockedTpmMessageView::LockedTpmMessageView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kVerticalBorderDp, kHorizontalBorderDp),
      kChildrenSpacingDp));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetPreferredSize(gfx::Size(kWidthDp, kHeightDp));
  SetFocusBehavior(FocusBehavior::ALWAYS);

  SetBackground(views::CreateThemedRoundedRectBackground(
      kColorAshShieldAndBaseOpaque, kRoundedCornerRadiusDp, 0));

  message_icon_ = AddChildView(std::make_unique<views::ImageView>());
  message_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kLockScreenAlertIcon, kColorAshIconColorPrimary, kIconSizeDp));

  message_warning_ = CreateLabel();
  message_warning_->SetEnabledColorId(kColorAshTextColorPrimary);

  message_description_ = CreateLabel();

  // Set content.
  std::u16string message_description =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_TPM_LOCKED_ISSUE_DESCRIPTION);
  message_description_->SetText(message_description);
  message_description_->SetEnabledColorId(kColorAshTextColorPrimary);
}

LockedTpmMessageView::~LockedTpmMessageView() = default;

void LockedTpmMessageView::SetRemainingTime(base::TimeDelta time_left) {
  std::u16string time_left_message;
  if (base::TimeDurationFormatWithSeconds(
          time_left, base::DurationFormatWidth::DURATION_WIDTH_WIDE,
          &time_left_message)) {
    std::u16string message_warning = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_POD_TPM_LOCKED_ISSUE_WARNING, time_left_message);
    message_warning_->SetText(message_warning);

    if (time_left.InMinutes() != prev_time_left_.InMinutes()) {
      message_warning_->GetViewAccessibility().SetName(message_warning);
    }
    prev_time_left_ = time_left;
  }
}

void LockedTpmMessageView::RequestFocus() {
  message_warning_->RequestFocus();
}

views::Label* LockedTpmMessageView::CreateLabel() {
  auto label = std::make_unique<views::Label>(std::u16string(),
                                              views::style::CONTEXT_LABEL,
                                              views::style::STYLE_PRIMARY);
  label->SetFontList(gfx::FontList().Derive(kDeltaDp, gfx::Font::NORMAL,
                                            gfx::Font::Weight::NORMAL));
  label->SetSubpixelRenderingEnabled(false);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFocusBehavior(FocusBehavior::ALWAYS);
  label->SetMultiLine(true);
  return AddChildView(std::move(label));
}

}  // namespace ash
