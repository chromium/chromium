// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/pin_status_view.h"

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_common.h"
#include "ash/auth/views/auth_view_utils.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login/login_utils.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

// Distance between the top of the view and the label.
constexpr int kTopLabelDistanceDp = 28;

std::u16string BuildPinStatusMessage(cryptohome::PinStatus* pin_status) {
  if (pin_status == nullptr || !pin_status->IsLockedFactor()) {
    return u"";
  }

  if (pin_status->AvailableAt().is_max()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_IN_SESSION_AUTH_PIN_TOO_MANY_ATTEMPTS);
  }

  base::TimeDelta delta = pin_status->AvailableAt() - base::Time::Now();
  std::u16string time_left_message;
  if (base::TimeDurationCompactFormatWithSeconds(
          delta, base::DurationFormatWidth::DURATION_WIDTH_WIDE,
          &time_left_message)) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_IN_SESSION_AUTH_PIN_DELAY_REQUIRED, time_left_message);
  }

  return l10n_util::GetStringUTF16(
      IDS_ASH_IN_SESSION_AUTH_PIN_TOO_MANY_ATTEMPTS);
}

}  // namespace

namespace ash {

PinStatusView::TestApi::TestApi(PinStatusView* view) : view_(view) {}
PinStatusView::TestApi::~TestApi() = default;

const std::u16string& PinStatusView::TestApi::GetCurrentText() const {
  return view_->GetCurrentText();
}

raw_ptr<views::Label> PinStatusView::TestApi::GetTextLabel() const {
  return view_->text_label_;
}

raw_ptr<PinStatusView> PinStatusView::TestApi::GetView() {
  return view_;
}

PinStatusView::PinStatusView() {
  auto decorate_label = [](views::Label* label) {
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  };

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(layout));

  // Add space.
  AddVerticalSpace(this, kTopLabelDistanceDp);

  // Add text.
  text_label_ = new views::Label(u"", views::style::CONTEXT_LABEL,
                                 views::style::STYLE_PRIMARY);
  text_label_->SetMultiLine(true);
  text_label_->SizeToFit(kTextLineWidthDp);
  text_label_->SetEnabledColorId(kTextColorId);
  text_label_->SetFontList(
      TypographyProvider::Get()->ResolveTypographyToken(kTextFont));
  decorate_label(text_label_);
  AddChildView(text_label_.get());
}

PinStatusView::~PinStatusView() {
  text_label_ = nullptr;
}

gfx::Size PinStatusView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_height =
      kTopLabelDistanceDp + text_label_->GetHeightForWidth(kTextLineWidthDp);
  return gfx::Size(kTextLineWidthDp, preferred_height);
}

void PinStatusView::SetText(const std::u16string& text_str) {
  SetVisible(!text_str.empty());
  text_label_->SetText(text_str);
}

const std::u16string& PinStatusView::GetCurrentText() const {
  return text_label_->GetText();
}

void PinStatusView::SetPinStatus(
    std::unique_ptr<cryptohome::PinStatus> pin_status) {
  lockout_timer_.Stop();

  pin_status_ = std::move(pin_status);

  const std::u16string status_message =
      BuildPinStatusMessage(pin_status_.get());
  SetText(status_message);
  text_label_->GetViewAccessibility().SetName(status_message);

  if (pin_status_ == nullptr) {
    return;
  }
  if (!pin_status_->IsLockedFactor()) {
    return;
  }

  text_label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                        /*send_native_event=*/true);

  if (pin_status_->AvailableAt().is_max()) {
    return;
  }

  // We need to update the label every second. The timer is scheduled to be run
  // more often to avoid possibly missing some updates due to timer imprecision.
  lockout_timer_.Start(FROM_HERE, base::Milliseconds(500),
                       base::BindRepeating(&PinStatusView::UpdateLockoutStatus,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void PinStatusView::UpdateLockoutStatus() {
  CHECK(pin_status_);
  SetText(BuildPinStatusMessage(pin_status_.get()));

  if (!pin_status_->IsLockedFactor()) {
    lockout_timer_.Stop();
  }
}

BEGIN_METADATA(PinStatusView)
END_METADATA

}  // namespace ash
