// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_TIME_VIEW_H_
#define ASH_SYSTEM_TIME_TIME_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/model/clock_observer.h"
#include "ash/system/tray/actionable_view.h"
#include "base/i18n/time_formatting.h"
#include "base/timer/timer.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/shadow_value.h"
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
 public:
  VerticalDateView();
  VerticalDateView(const VerticalDateView& other) = delete;
  VerticalDateView& operator=(const VerticalDateView& other) = delete;
  ~VerticalDateView() override;

  // views::View:
  void OnThemeChanged() override;

  // Updates the date label text.
  void UpdateText();

 private:
  friend class TimeViewTest;

  views::ImageView* icon_ = nullptr;
  views::Label* text_label_ = nullptr;
};

// Tray view used to display the current date or time based on the passed in
// `Type`. Exported for tests.
class ASH_EXPORT TimeView : public ActionableView, public ClockObserver {
 public:
  METADATA_HEADER(TimeView);

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

  // ClockObserver:
  void OnDateFormatChanged() override;
  void OnSystemClockTimeUpdated() override;
  void OnSystemClockCanSetTimeChanged(bool can_set_time) override;
  void Refresh() override;

  base::HourClockType GetHourTypeForTesting() const;

  views::Label* horizontal_label_for_test() { return horizontal_label_; }
  views::Label* horizontal_label_date_for_test() {
    return horizontal_label_date_;
  }

 private:
  friend class TimeViewTest;
  friend class TimeTrayItemViewTest;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
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

  // Subviews used for different layouts.
  // When either of the subviews is in use, it transfers the ownership to the
  // views hierarchy and becomes nullptr.
  std::unique_ptr<views::View> horizontal_view_;
  std::unique_ptr<views::View> vertical_view_;

  // Label text used for the normal horizontal shelf.
  views::Label* horizontal_label_ = nullptr;
  views::Label* horizontal_label_date_ = nullptr;

  // The horizontal and vertical date view for the `DateTray`.
  std::unique_ptr<views::View> horizontal_date_view_;
  std::unique_ptr<views::View> vertical_date_view_;

  // The time label is split into two lines for the vertical shelf.
  views::Label* vertical_label_hours_ = nullptr;
  views::Label* vertical_label_minutes_ = nullptr;

  // The vertical date in a calendar icon view for the vertical shelf.
  VerticalDateView* date_view_ = nullptr;

  // Invokes UpdateText() when the displayed time should change.
  base::OneShotTimer timer_;

  ClockModel* const model_;

  // The type (kDate or kTime) of this time view.
  const Type type_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_TIME_VIEW_H_
