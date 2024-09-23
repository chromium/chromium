// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_TIME_VIEW_H_
#define ASH_SYSTEM_TIME_TIME_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/model/clock_observer.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace base {
class Time;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class ClockModel;

// The Date view, which is a date in a calendar icon, for vertical time view.
// For horizontal time view, there's no Date Icon View and it shows a text date.
class VerticalDateView : public views::View {
  METADATA_HEADER(VerticalDateView, views::View)

 public:
  VerticalDateView();
  VerticalDateView(const VerticalDateView& other) = delete;
  VerticalDateView& operator=(const VerticalDateView& other) = delete;
  ~VerticalDateView() override;

  // Updates the date label text.
  void UpdateText();

  // For Jelly: updates `icon_` and `text_label_` color ids.
  void UpdateIconAndLabelColorId(ui::ColorId color_id);

 private:
  friend class TimeViewTest;

  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> text_label_ = nullptr;
};

// Tray view used to display the current date or time based on the passed in
// `Type`. Exported for tests.
class ASH_EXPORT TimeView : public views::View, public ClockObserver {
  METADATA_HEADER(TimeView, views::View)

 public:
  enum class ClockLayout {
    HORIZONTAL_CLOCK,
    VERTICAL_CLOCK,
  };

  enum Type { kTime, kDate };

  TimeView(ClockLayout clock_layout, ClockModel* model, Type type = kTime);

  TimeView(const TimeView&) = delete;
  TimeView& operator=(const TimeView&) = delete;

  ~TimeView() override;

  // Updates clock layout.
  void UpdateClockLayout(ClockLayout clock_layout);

  // Updates the time text color id.
  void SetTextColorId(ui::ColorId color_id,
                      bool auto_color_readability_enabled = false);

  // Updates the text color.
  void SetTextColor(SkColor color, bool auto_color_readability_enabled = false);

  // Updates the time text fontlist.
  void SetTextFont(const gfx::FontList& font_list);

  // Updates the time text shadow values.
  void SetTextShadowValues(const gfx::ShadowValues& shadows);

  // For Jelly: updates `vertical_date_view_` color id if exists.
  void SetDateViewColorId(ui::ColorId color_id);

  // Controls whether the horizontal time view shows "AM/PM" text.
  // This setting does not affect the vertical time view.
  void SetAmPmClockType(base::AmPmClockType am_pm_clock_type);

  // ClockObserver:
  void OnDateFormatChanged() override;
  void OnSystemClockTimeUpdated() override;
  void OnSystemClockCanSetTimeChanged(bool can_set_time) override;
  void Refresh() override;

  base::AmPmClockType GetAmPmClockTypeForTesting() const {
    return am_pm_clock_type_;
  }

  base::HourClockType GetHourTypeForTesting() const;

  views::Label* GetHorizontalTimeLabelForTesting() {
    return horizontal_time_label_;
  }

  views::Label* GetHorizontalDateLabelForTesting() {
    return horizontal_date_label_;
  }

  views::Label* GetVerticalMinutesLabelForTesting() {
    return vertical_label_minutes_;
  }

  views::Label* GetVerticalHoursLabelForTesting() {
    return vertical_label_hours_;
  }

 private:
  friend class TimeViewTest;
  friend class TimeTrayItemViewTest;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Updates the displayed text for the current time and calls SetTimer().
  void UpdateText();

  // Updates the format of the displayed time.
  void UpdateTimeFormat();

  // Updates labels to display the current time.
  void UpdateTextInternal(const base::Time& now);

  void SetupDateviews(ClockLayout clock_layout);
  void SetupSubviews(ClockLayout clock_layout);
  void SetupLabel(views::Label* label);

  // Starts |timer_| to schedule the next update.
  void SetTimer(const base::Time& now);

  // Indicates if the horizontal view should show "AM/PM" text next to the time.
  base::AmPmClockType am_pm_clock_type_ = base::kDropAmPm;

  // The `TimeView` of `Type::kTime` shows a single label in horizontal shelf,
  // or two stacked labels in vertical shelf. The container views own the
  // associated labels for vertical/horizontal, and the container views are
  // owned by this view by the views hierarchy.
  raw_ptr<views::View> horizontal_time_label_container_ = nullptr;
  raw_ptr<views::Label> horizontal_time_label_ = nullptr;
  raw_ptr<views::View> vertical_time_label_container_ = nullptr;
  raw_ptr<views::Label> vertical_label_hours_ = nullptr;
  raw_ptr<views::Label> vertical_label_minutes_ = nullptr;

  // The `TimeView` of `Type::kDate` shows a single date label in horizontal
  // shelf, or a calendar image with a date number in vertical shelf.  The
  // container views own the associated views for vertical/horizontal, and the
  // container views are owned by this view by the views hierarchy.
  raw_ptr<views::View> horizontal_date_label_container_ = nullptr;
  raw_ptr<views::Label, DanglingUntriaged> horizontal_date_label_ = nullptr;
  raw_ptr<views::View> vertical_date_view_container_ = nullptr;
  raw_ptr<VerticalDateView> vertical_date_view_ = nullptr;

  // Invokes UpdateText() when the displayed time should change.
  base::OneShotTimer timer_;

  const raw_ptr<ClockModel> model_;

  // The type (kDate or kTime) of this time view.
  const Type type_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_TIME_VIEW_H_
