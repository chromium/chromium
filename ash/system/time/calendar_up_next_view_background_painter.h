// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_BACKGROUND_PAINTER_H_
#define ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_BACKGROUND_PAINTER_H_

#include "ui/views/background.h"

class SkPath;

namespace gfx {
class Canvas;
}

namespace ash {

class CalendarUpNextViewBackground : public views::Background {
 public:
  explicit CalendarUpNextViewBackground(ui::ColorId color_id);
  CalendarUpNextViewBackground(const CalendarUpNextViewBackground& other) =
      delete;
  CalendarUpNextViewBackground& operator=(
      const CalendarUpNextViewBackground& other) = delete;
  ~CalendarUpNextViewBackground() override;

  static SkPath GetPath(const gfx::Size& size);

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;
  void OnViewThemeChanged(views::View* view) override;

 private:
  ui::ColorId color_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UP_NEXT_VIEW_BACKGROUND_PAINTER_H_
