// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_UP_NEXT_EVENT_ITEM_VIEW_H_
#define ASH_GLANCEABLES_GLANCEABLES_UP_NEXT_EVENT_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// Represents a single calendar event inside "Up next" section. Opens calendar
// app on click.
class ASH_EXPORT GlanceablesUpNextEventItemView : public views::Button {
 public:
  explicit GlanceablesUpNextEventItemView(
      google_apis::calendar::CalendarEvent event);
  GlanceablesUpNextEventItemView(const GlanceablesUpNextEventItemView&) =
      delete;
  GlanceablesUpNextEventItemView& operator=(
      const GlanceablesUpNextEventItemView&) = delete;
  ~GlanceablesUpNextEventItemView() override = default;

  views::Label* event_title_label_for_test() { return event_title_label_; }
  views::Label* event_time_label_for_test() { return event_time_label_; }

 private:
  friend class GlanceablesTest;

  void OpenEvent() const;

  google_apis::calendar::CalendarEvent event_;
  raw_ptr<views::Label, ExperimentalAsh> event_title_label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> event_time_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_UP_NEXT_EVENT_ITEM_VIEW_H_
