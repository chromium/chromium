// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_TIME_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_TIME_TIME_TRAY_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/macros.h"

namespace ash {
class Shelf;

namespace tray {

class TimeView;

class TimeTrayItemView : public TrayItemView, public SessionObserver {
 public:
  explicit TimeTrayItemView(Shelf* shelf);
  ~TimeTrayItemView() override;

  void UpdateAlignmentForShelf(Shelf* shelf);
  tray::TimeView* time_view() { return time_view_; }

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  TimeView* time_view_ = nullptr;
  ScopedSessionObserver session_observer_;
  DISALLOW_COPY_AND_ASSIGN(TimeTrayItemView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_TIME_TIME_TRAY_ITEM_VIEW_H_
