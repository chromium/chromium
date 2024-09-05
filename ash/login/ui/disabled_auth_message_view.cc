// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/disabled_auth_message_view.h"

#include <memory>
#include <string>

#include "ash/public/cpp/login_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr int kVerticalBorderDp = 16;
constexpr int kHorizontalBorderDp = 16;
constexpr int kChildrenSpacingDp = 4;
constexpr int kTimeWidthDp = 204;
constexpr int kMultiprofileWidthDp = 304;
constexpr int kIconSizeDp = 24;
constexpr int kTitleFontSizeDeltaDp = 3;
constexpr int kContentsFontSizeDeltaDp = -1;
constexpr int kRoundedCornerRadiusDp = 8;

// The content needed to render the disabled auth message view.
struct LockScreenMessage {
  std::u16string title;
  std::u16string content;
  ui::ImageModel icon;
};

// Returns the message used when the device was locked due to a time window
// limit.
LockScreenMessage GetWindowLimitMessage(const base::Time& unlock_time,
                                        bool use_24hour_clock) {
  LockScreenMessage message;
  message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TIME_FOR_BED_MESSAGE);

  base::Time local_midnight = base::Time::Now().LocalMidnight();

  std::u16string time_to_display;
  if (use_24hour_clock) {
    time_to_display = base::TimeFormatTimeOfDayWithHourClockType(
        unlock_time, base::k24HourClock, base::kDropAmPm);
  } else {
    time_to_display = base::TimeFormatTimeOfDayWithHourClockType(
        unlock_time, base::k12HourClock, base::kKeepAmPm);
  }

  if (unlock_time < local_midnight + base::Days(1)) {
    // Unlock time is today.
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_MESSAGE, time_to_display);
  } else if (unlock_time < local_midnight + base::Days(2)) {
    // Unlock time is tomorrow.
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_TOMORROW_MESSAGE, time_to_display);
  } else {
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_DAY_OF_WEEK_MESSAGE,
        base::LocalizedTimeFormatWithPattern(unlock_time, "EEEE"),
        time_to_display);
  }
  message.icon = ui::ImageModel::FromVectorIcon(
      kLockScreenTimeLimitMoonIcon, kColorAshIconColorPrimary, kIconSizeDp);
  return message;
}

// Returns the message used when the device was locked due to a time usage
// limit.
LockScreenMessage GetUsageLimitMessage(const base::TimeDelta& used_time) {
  LockScreenMessage message;

  // 1 minute is used instead of 0, because the device is used for a few
  // milliseconds before locking.
  if (used_time < base::Minutes(1)) {
    // The device was locked all day.
    message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TAKE_BREAK_MESSAGE);
    message.content =
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_LOCKED_ALL_DAY_MESSAGE);
  } else {
    // The usage limit is over.
    message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TIME_IS_UP_MESSAGE);

    const std::u16string used_time_string = ui::TimeFormat::Detailed(
        ui::TimeFormat::Format::FORMAT_DURATION,
        ui::TimeFormat::Length::LENGTH_LONG, /*cutoff=*/3, used_time);
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_SCREEN_TIME_USED_MESSAGE, used_time_string);
  }
  message.icon = ui::ImageModel::FromVectorIcon(
      kLockScreenTimeLimitTimerIcon, kColorAshIconColorPrimary, kIconSizeDp);
  return message;
}

// Returns the message used when the device was locked due to a time limit
// override.
LockScreenMessage GetOverrideMessage() {
  LockScreenMessage message;
  message.title =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TIME_FOR_A_BREAK_MESSAGE);
  message.content =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_MANUAL_LOCK_MESSAGE);
  message.icon = ui::ImageModel::FromVectorIcon(
      kLockScreenTimeLimitLockIcon, kColorAshIconColorPrimary, kIconSizeDp);
  return message;
}

LockScreenMessage GetLockScreenMessage(AuthDisabledReason lock_reason,
                                       const base::Time& unlock_time,
                                       const base::TimeDelta& used_time,
                                       bool use_24hour_clock) {
  switch (lock_reason) {
    case AuthDisabledReason::kTimeWindowLimit:
      return GetWindowLimitMessage(unlock_time, use_24hour_clock);
    case AuthDisabledReason::kTimeUsageLimit:
      return GetUsageLimitMessage(used_time);
    case AuthDisabledReason::kTimeLimitOverride:
      return GetOverrideMessage();
    default:
      NOTREACHED();
  }
}

}  // namespace

DisabledAuthMessageView::TestApi::TestApi(DisabledAuthMessageView* view)
    : view_(view) {}

DisabledAuthMessageView::TestApi::~TestApi() = default;

const std::u16string&
DisabledAuthMessageView::TestApi::GetDisabledAuthMessageContent() const {
  return view_->message_contents_->GetText();
}

void DisabledAuthMessageView::TestApi::SetDisabledAuthMessageTitleForTesting(
    std::u16string message_title) {
  view_->SetAuthDisabledMessage(message_title, u"");
}

DisabledAuthMessageView::DisabledAuthMessageView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kVerticalBorderDp, kHorizontalBorderDp),
      kChildrenSpacingDp));
  preferred_width_ = kTimeWidthDp;
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetBackground(views::CreateThemedRoundedRectBackground(
      kColorAshShieldAndBaseOpaque, kRoundedCornerRadiusDp));

  // The icon size has to be defined later if the image will be visible.
  message_icon_ = AddChildView(std::make_unique<views::ImageView>());
  message_icon_->SetImageSize(gfx::Size(kIconSizeDp, kIconSizeDp));

  auto decorate_label = [](views::Label* label) {
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetFocusBehavior(FocusBehavior::ALWAYS);
  };
  message_title_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  message_title_->SetFontList(gfx::FontList().Derive(
      kTitleFontSizeDeltaDp, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  decorate_label(message_title_);
  message_title_->SetEnabledColorId(kColorAshTextColorPrimary);

  message_contents_ = AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  message_contents_->SetFontList(gfx::FontList().Derive(
      kContentsFontSizeDeltaDp, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
  decorate_label(message_contents_);
  message_contents_->SetMultiLine(true);
  message_contents_->SetEnabledColorId(kColorAshTextColorPrimary);

  GetViewAccessibility().SetRole(ax::mojom::Role::kPane);
  UpdateAccessibleName();
  message_title_changed_subscription_ = message_title_->AddTextChangedCallback(
      base::BindRepeating(&DisabledAuthMessageView::OnMessageTitleChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

DisabledAuthMessageView::~DisabledAuthMessageView() = default;

void DisabledAuthMessageView::SetAuthDisabledMessage(
    const AuthDisabledData& auth_disabled_data,
    bool use_24hour_clock) {
  LockScreenMessage message = GetLockScreenMessage(
      auth_disabled_data.reason, auth_disabled_data.auth_reenabled_time,
      auth_disabled_data.device_used_time, use_24hour_clock);
  preferred_width_ = kTimeWidthDp;
  message_icon_->SetVisible(true);
  CHECK(!message.icon.IsEmpty());
  message_icon_->SetImage(message.icon);
  message_title_->SetText(message.title);
  message_title_->SetEnabledColorId(kColorAshTextColorPrimary);
  message_contents_->SetText(message.content);
  message_contents_->SetEnabledColorId(kColorAshTextColorPrimary);
}

void DisabledAuthMessageView::SetAuthDisabledMessage(
    const std::u16string& title,
    const std::u16string& content) {
  preferred_width_ = kMultiprofileWidthDp;
  message_icon_->SetVisible(false);
  message_title_->SetText(title);
  message_contents_->SetText(content);
}

void DisabledAuthMessageView::RequestFocus() {
  message_title_->RequestFocus();
}

gfx::Size DisabledAuthMessageView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, preferred_width_);
  return gfx::Size(preferred_width_, height);
}

void DisabledAuthMessageView::OnMessageTitleChanged() {
  UpdateAccessibleName();
}

void DisabledAuthMessageView::UpdateAccessibleName() {
  // Any view which claims to be focusable is expected to have an accessible
  // name so that screen readers know what to present to the user when it gains
  // focus. In the case of this particular view, `RequestFocus` gives focus to
  // the `message_title_` label. As a result, this view is not end-user
  // focusable. However, its official focusability will cause the accessibility
  // paint checks to fail. If the `message_title_` has text, set the accessible
  // name to that text; otherwise set the name explicitly empty to prevent
  // the paint check from failing during tests.
  if (message_title_->GetText().empty()) {
    GetViewAccessibility().SetName(
        std::string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  } else {
    GetViewAccessibility().SetName(message_title_->GetText());
  }
}

}  // namespace ash
