// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/pin_status_message_view.h"

#include <memory>
#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/i18n/time_formatting.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kVerticalBorderDp = 20;
constexpr int kHorizontalBorderDp = 0;
constexpr int kWidthDp = 320;
constexpr int kHeightDp = 120;
constexpr int kDeltaDp = 0;
// The time interval to refresh the pin delay message.
constexpr base::TimeDelta kRefreshInterval = base::Milliseconds(200);

bool TimeDurationFormat(base::TimeDelta time, std::u16string* out) {
  if (!time.is_positive()) {
    return false;
  }
  // Shows "x hours, y minutes" when the time is more than an hour, otherwise
  // shows "x minutes, y seconds".
  if (time.InHours() >= 1) {
    return base::TimeDurationFormat(
        time, base::DurationFormatWidth::DURATION_WIDTH_WIDE, out);
  } else {
    return base::TimeDurationCompactFormatWithSeconds(
        time, base::DurationFormatWidth::DURATION_WIDTH_WIDE, out);
  }
}
}  // namespace

PinStatusMessageView::TestApi::TestApi(PinStatusMessageView* view)
    : view_(view) {}

PinStatusMessageView::TestApi::~TestApi() = default;

const std::u16string&
PinStatusMessageView::TestApi::GetPinStatusMessageContent() const {
  return view_->message_->GetText();
}

PinStatusMessageView::PinStatusMessageView(base::RepeatingClosure on_pin_unlock)
    : on_pin_unlock_(on_pin_unlock) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kVerticalBorderDp, kHorizontalBorderDp)));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetPreferredSize(gfx::Size(kWidthDp, kHeightDp));

  message_ = new views::Label(std::u16string(), views::style::CONTEXT_LABEL,
                              views::style::STYLE_PRIMARY);
  message_->SetFontList(gfx::FontList().Derive(kDeltaDp, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
  message_->SetSubpixelRenderingEnabled(false);
  message_->SetAutoColorReadabilityEnabled(false);
  message_->SetMultiLine(true);
  message_->SetEnabledColorId(kColorAshTextColorPrimary);
  message_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  message_->GetViewAccessibility().SetName(
      std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  AddChildView(message_.get());
}

PinStatusMessageView::~PinStatusMessageView() {
  message_ = nullptr;
}

void PinStatusMessageView::SetPinInfo(base::Time available_at,
                                      bool is_pin_only) {
  available_at_ = available_at;
  is_pin_only_ = is_pin_only;
  UpdateUI();
  timer_.Start(FROM_HERE, kRefreshInterval, this,
               &PinStatusMessageView::UpdateUI, base::TimeTicks::Now());
}

void PinStatusMessageView::UpdateUI() {
  base::TimeDelta time_left = available_at_ - base::Time::Now();
  if (!time_left.is_positive()) {
    on_pin_unlock_.Run();
    message_->SetText(std::u16string());
    timer_.Stop();
    return;
  }
  std::u16string time_left_message;
  if (TimeDurationFormat(time_left, &time_left_message)) {
    std::u16string message_warning = l10n_util::GetStringFUTF16(
        is_pin_only_ ? IDS_ASH_LOGIN_POD_PIN_LOCKED_NO_PASSWORD_WARNING
                     : IDS_ASH_LOGIN_POD_PIN_LOCKED_WARNING,
        time_left_message);
    if (is_pin_only_) {
      base::StrAppend(
          &message_warning,
          {u"\n\n", l10n_util::GetStringUTF16(
                        IDS_ASH_LOGIN_POD_PIN_LOCKED_RECOVERY_PROMPT)});
    }
    message_->SetText(message_warning);
  }
}

void PinStatusMessageView::RequestFocus() {
  message_->RequestFocus();
}

}  // namespace ash
