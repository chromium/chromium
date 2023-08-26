// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_view.h"

#include <memory>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

// Padding between the left edge of the shelf and the left edge of the vertical
// clock.
const int kVerticalClockLeftPadding = 9;

// Padding between the left/right edge of the shelf and the left edge of the
// vertical clock with date.
const int kVerticalDateClockHorizontalPadding = 8;

// Padding on top/bottom of the vertical clock date view.
const int kVerticalDateVerticalPadding = 2;

// How much size smaller the text in the date view compare to the text size of
// the clock view.
const int kDateTextSizeDiff = 4;

// Offset used to bring the minutes line closer to the hours line in the
// vertical clock.
const int kVerticalClockMinutesTopOffset = -2;

std::u16string FormatDate(const base::Time& time) {
  // Use 'short' month format (e.g., "Oct") followed by non-padded day of
  // month (e.g., "2", "10").
  return base::LocalizedTimeFormatWithPattern(time, "LLLd");
}

// Returns the time to show by the time view.
base::Time GetTimeToShow() {
  if (!switches::IsStabilizeTimeDependentViewForTestsEnabled())
    return base::Time::Now();

  // The code below only runs in tests.
  static base::Time fixed_time;
  if (fixed_time.is_null())
    CHECK(base::Time::FromString(kFakeNowTimeStringInPixelTest, &fixed_time));

  return fixed_time;
}

}  // namespace

VerticalDateView::VerticalDateView()
    : icon_(AddChildView(std::make_unique<views::ImageView>())),
      text_label_(AddChildView(std::make_unique<views::Label>())) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  text_label_->SetSubpixelRenderingEnabled(false);
  text_label_->SetAutoColorReadabilityEnabled(false);
  text_label_->SetFontList(
      gfx::FontList().Derive(kTrayTextFontSizeIncrease - kDateTextSizeDiff,
                             gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
  text_label_->SetElideBehavior(gfx::NO_ELIDE);
  UpdateText();
  text_label_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kVerticalDateVerticalPadding, 0, 0, 0)));
  UpdateIconAndLabelColorId(cros_tokens::kCrosSysOnSurface);
}

VerticalDateView::~VerticalDateView() = default;

void VerticalDateView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // For Jelly: color ids are already set and theme change will be handled
  // automatically.
  if (chromeos::features::IsJellyEnabled()) {
    return;
  }

  text_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  icon_->SetImage(gfx::CreateVectorIcon(
      kCalendarBackgroundIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
}

void VerticalDateView::UpdateText() {
  const base::Time time_to_show = GetTimeToShow();
  const std::u16string new_text = calendar_utils::GetDayIntOfMonth(
      time_to_show + calendar_utils::GetTimeDifference(time_to_show));
  if (text_label_->GetText() == new_text)
    return;
  text_label_->SetText(new_text);
  text_label_->SetTooltipText(base::TimeFormatFriendlyDate(time_to_show));
}

void VerticalDateView::UpdateIconAndLabelColorId(ui::ColorId color_id) {
  if (!chromeos::features::IsJellyEnabled()) {
    return;
  }

  text_label_->SetEnabledColorId(color_id);
  icon_->SetImage(
      ui::ImageModel::FromVectorIcon(kCalendarBackgroundIcon, color_id));
}

TimeView::TimeView(ClockLayout clock_layout, ClockModel* model, Type type)
    : ActionableView(TrayPopupInkDropStyle::INSET_BOUNDS),
      model_(model),
      type_(type) {
  SetTimer(GetTimeToShow());
  SetFocusBehavior(FocusBehavior::NEVER);
  model_->AddObserver(this);
  switch (type_) {
    case kTime:
      SetupSubviews(clock_layout);
      break;
    case kDate:
      SetupDateviews(clock_layout);
      break;
  }
  UpdateTextInternal(GetTimeToShow());
}

TimeView::~TimeView() {
  model_->RemoveObserver(this);
  timer_.Stop();
}

void TimeView::UpdateClockLayout(ClockLayout clock_layout) {
  switch (type_) {
    case kDate: {
      // Do nothing if the layout hasn't changed.
      if (clock_layout == ClockLayout::HORIZONTAL_CLOCK
              ? vertical_date_view_
              : horizontal_date_view_) {
        return;
      }

      if (clock_layout == ClockLayout::HORIZONTAL_CLOCK) {
        vertical_date_view_ = RemoveChildViewT(children()[0]);
        AddChildView(std::move(horizontal_date_view_));
      } else {
        horizontal_date_view_ = RemoveChildViewT(children()[0]);
        AddChildView(std::move(vertical_date_view_));
      }
      break;
    }
    case kTime: {
      // Do nothing if the layout hasn't changed.
      if (clock_layout == ClockLayout::HORIZONTAL_CLOCK ? vertical_view_
                                                        : horizontal_view_) {
        return;
      }

      if (clock_layout == ClockLayout::HORIZONTAL_CLOCK) {
        vertical_view_ = RemoveChildViewT(children()[0]);
        AddChildView(std::move(horizontal_view_));
      } else {
        horizontal_view_ = RemoveChildViewT(children()[0]);
        AddChildView(std::move(vertical_view_));
      }
      break;
    }
  }
  Layout();
}

void TimeView::SetTextColorId(ui::ColorId color_id,
                              bool auto_color_readability_enabled) {
  auto set_color_id = [&](views::Label* label) {
    label->SetEnabledColorId(color_id);
    label->SetAutoColorReadabilityEnabled(auto_color_readability_enabled);
  };

  switch (type_) {
    case kTime:
      set_color_id(horizontal_label_);
      set_color_id(vertical_label_hours_);
      set_color_id(vertical_label_minutes_);
      return;
    case kDate:
      set_color_id(horizontal_label_date_);
  }
}

void TimeView::SetTextColor(SkColor color,
                            bool auto_color_readability_enabled) {
  auto set_color = [&](views::Label* label) {
    label->SetEnabledColor(color);
    label->SetAutoColorReadabilityEnabled(auto_color_readability_enabled);
  };

  switch (type_) {
    case kTime:
      set_color(horizontal_label_);
      set_color(vertical_label_hours_);
      set_color(vertical_label_minutes_);
      return;
    case kDate:
      set_color(horizontal_label_date_);
  }
}

void TimeView::SetTextFont(const gfx::FontList& font_list) {
  switch (type_) {
    case kTime:
      horizontal_label_->SetFontList(font_list);
      vertical_label_hours_->SetFontList(font_list);
      vertical_label_minutes_->SetFontList(font_list);
      return;
    case kDate:
      horizontal_label_date_->SetFontList(font_list);
  }
}

void TimeView::SetTextShadowValues(const gfx::ShadowValues& shadows) {
  switch (type_) {
    case kTime:
      horizontal_label_->SetShadows(shadows);
      vertical_label_hours_->SetShadows(shadows);
      vertical_label_minutes_->SetShadows(shadows);
      return;
    case kDate:
      horizontal_label_date_->SetShadows(shadows);
  }
}

void TimeView::SetDateViewColorId(ui::ColorId color_id) {
  if (chromeos::features::IsJellyEnabled() && date_view_) {
    date_view_->UpdateIconAndLabelColorId(color_id);
  }
}

void TimeView::OnDateFormatChanged() {
  UpdateTimeFormat();
}

void TimeView::OnSystemClockTimeUpdated() {
  UpdateTimeFormat();
}

void TimeView::OnSystemClockCanSetTimeChanged(bool can_set_time) {}

void TimeView::Refresh() {
  UpdateText();
}

base::HourClockType TimeView::GetHourTypeForTesting() const {
  return model_->hour_clock_type();
}

bool TimeView::PerformAction(const ui::Event& event) {
  return false;
}

void TimeView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kTime;
}

void TimeView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

bool TimeView::OnMousePressed(const ui::MouseEvent& event) {
  // Let the event fall through.
  return false;
}

void TimeView::OnGestureEvent(ui::GestureEvent* event) {
  // Skip gesture handling happening in Button so that the container views
  // receive and handle them properly.
}

void TimeView::UpdateText() {
  const base::Time now = GetTimeToShow();
  UpdateTextInternal(now);
  SchedulePaint();
  SetTimer(now);
}

void TimeView::UpdateTimeFormat() {
  UpdateText();
}

void TimeView::UpdateTextInternal(const base::Time& now) {
  // Just in case |now| is null, do NOT update time; otherwise, it will
  // crash icu code by calling into base::TimeFormatTimeOfDayWithHourClockType,
  // see details in crbug.com/147570.
  if (now.is_null()) {
    LOG(ERROR) << "Received null value from base::Time |now| in argument";
    return;
  }
  const std::u16string friendly_format_date = base::TimeFormatFriendlyDate(now);
  SetAccessibleName(base::TimeFormatTimeOfDayWithHourClockType(
                        now, model_->hour_clock_type(), base::kKeepAmPm) +
                    u", " + friendly_format_date);

  switch (type_) {
    case kTime: {
      // Calculate horizontal clock layout label.
      const std::u16string current_time =
          base::TimeFormatTimeOfDayWithHourClockType(
              now, model_->hour_clock_type(), base::kDropAmPm);

      const bool label_length_changed =
          horizontal_label_->GetText().length() != current_time.length();
      horizontal_label_->SetText(current_time);
      horizontal_label_->SetTooltipText(friendly_format_date);

      // Calculate vertical clock layout labels.
      std::u16string current_hours =
          (model_->hour_clock_type() == base::k24HourClock)
              ? calendar_utils::GetTwentyFourHourClockHours(now)
              : calendar_utils::GetTwelveHourClockHours(now);
      const std::u16string current_minutes = calendar_utils::GetMinutes(now);

      vertical_label_hours_->SetText(current_hours);
      vertical_label_minutes_->SetText(current_minutes);

      Layout();

      // When the `new_label` text does not have the some length as the
      // old one's, the layout size of this time view changes as well.
      if (label_length_changed)
        PreferredSizeChanged();

      return;
    }
    case kDate: {
      const std::u16string current_date = FormatDate(now);
      horizontal_label_date_->SetText(current_date);
      horizontal_label_date_->SetTooltipText(friendly_format_date);
      date_view_->UpdateText();
    }
  }
}

void TimeView::SetupDateviews(ClockLayout clock_layout) {
  DCHECK_EQ(type_, kDate);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  horizontal_date_view_ = std::make_unique<View>();
  horizontal_date_view_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  horizontal_date_view_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kUnifiedTrayTextTopPadding, kUnifiedTrayTimeLeftPadding, 0, 0)));
  horizontal_label_date_ =
      horizontal_date_view_->AddChildView(std::make_unique<views::Label>());
  SetupLabel(horizontal_label_date_);

  vertical_date_view_ = std::make_unique<View>();
  vertical_date_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
  date_view_ =
      vertical_date_view_->AddChildView(std::make_unique<VerticalDateView>());
  date_view_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 0, 0)));
  AddChildView(clock_layout == ClockLayout::HORIZONTAL_CLOCK
                   ? std::move(horizontal_date_view_)
                   : std::move(vertical_date_view_));
}

void TimeView::SetupSubviews(ClockLayout clock_layout) {
  DCHECK_EQ(type_, kTime);
  horizontal_view_ = std::make_unique<View>();
  horizontal_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
  horizontal_view_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kUnifiedTrayTextTopPadding, kUnifiedTrayTimeLeftPadding, 0, 0)));
  horizontal_label_ =
      horizontal_view_->AddChildView(std::make_unique<views::Label>());
  SetupLabel(horizontal_label_);

  vertical_view_ = std::make_unique<View>();
  vertical_view_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kEnd)
      .SetInteriorMargin(gfx::Insets::TLBR(0, kVerticalClockLeftPadding,
                                           kVerticalClockMinutesTopOffset, 0));

  vertical_label_hours_ =
      vertical_view_->AddChildView(std::make_unique<views::Label>());
  SetupLabel(vertical_label_hours_);
  vertical_label_hours_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(0, kVerticalDateClockHorizontalPadding)));

  vertical_label_minutes_ =
      vertical_view_->AddChildView(std::make_unique<views::Label>());
  SetupLabel(vertical_label_minutes_);
  // Pull the minutes up closer to the hours by using a negative top border.
  vertical_label_minutes_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kVerticalClockMinutesTopOffset, kVerticalDateClockHorizontalPadding, 0,
      kVerticalDateClockHorizontalPadding)));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(clock_layout == ClockLayout::HORIZONTAL_CLOCK
                   ? std::move(horizontal_view_)
                   : std::move(vertical_view_));
}

void TimeView::SetupLabel(views::Label* label) {
  SetupLabelForTray(label);
  label->SetElideBehavior(gfx::NO_ELIDE);
}

void TimeView::SetTimer(const base::Time& now) {
  // Try to set the timer to go off at the next change of the minute. We don't
  // want to have the timer go off more than necessary since that will cause
  // the CPU to wake up and consume power.
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);

  // Often this will be called at minute boundaries, and we'll actually want
  // 60 seconds from now.
  int seconds_left = 60 - exploded.second;
  if (seconds_left == 0)
    seconds_left = 60;

  // Make sure that the timer fires on the next minute. Without this, if it is
  // called just a teeny bit early, then it will skip the next minute.
  seconds_left += kTimerSlopSeconds;

  timer_.Stop();
  timer_.Start(FROM_HERE, base::Seconds(seconds_left), this,
               &TimeView::UpdateText);
}

BEGIN_METADATA(TimeView, ActionableView)
END_METADATA

}  // namespace ash
