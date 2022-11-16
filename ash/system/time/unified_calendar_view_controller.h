// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_UNIFIED_CALENDAR_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_TIME_UNIFIED_CALENDAR_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"

namespace ash {

class DetailedViewDelegate;
class UnifiedSystemTrayController;
class CalendarView;

// Controller of `CalendarView` in UnifiedSystemTray.
class UnifiedCalendarViewController : public DetailedViewController {
 public:
  explicit UnifiedCalendarViewController(
      UnifiedSystemTrayController* tray_controller);
  UnifiedCalendarViewController(const UnifiedCalendarViewController& other) =
      delete;
  UnifiedCalendarViewController& operator=(
      const UnifiedCalendarViewController& other) = delete;
  ~UnifiedCalendarViewController() override;

  // DetailedViewController:
  std::unique_ptr<views::View> CreateView() override;
  std::u16string GetAccessibleName() const override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  // Unowned, the object that instantiated us.
  UnifiedSystemTrayController* const tray_controller_;

  // Owned by UnifiedSystemTrayView's detailed_view_container_.
  CalendarView* view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_UNIFIED_CALENDAR_VIEW_CONTROLLER_H_
