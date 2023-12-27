// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_UNIFIED_CALENDAR_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_TIME_UNIFIED_CALENDAR_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class CalendarView;

// Controller of `CalendarView` in UnifiedSystemTray.
class UnifiedCalendarViewController : public DetailedViewController {
 public:
  UnifiedCalendarViewController() = default;
  UnifiedCalendarViewController(const UnifiedCalendarViewController& other) =
      delete;
  UnifiedCalendarViewController& operator=(
      const UnifiedCalendarViewController& other) = delete;
  ~UnifiedCalendarViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  // Owned by `QuickSettingsView`'s detailed_view_container_.
  raw_ptr<CalendarView, DanglingUntriaged> view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_UNIFIED_CALENDAR_VIEW_CONTROLLER_H_
