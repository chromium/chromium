// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_TIME_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_TIME_TIME_TRAY_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "time_view.h"

namespace ash {
class Shelf;

namespace tray {

class ASH_EXPORT TimeTrayItemView : public TrayItemView,
                                    public SessionObserver,
                                    public UnifiedSystemTrayModel::Observer {
 public:
  TimeTrayItemView(Shelf* shelf,
                   scoped_refptr<UnifiedSystemTrayModel> model,
                   absl::optional<TimeView::OnTimeViewActionPerformedCallback>
                       callback = absl::nullopt);

  TimeTrayItemView(const TimeTrayItemView&) = delete;
  TimeTrayItemView& operator=(const TimeTrayItemView&) = delete;

  ~TimeTrayItemView() override;

  void UpdateAlignmentForShelf(Shelf* shelf);
  TimeView* time_view() { return time_view_; }

  // TrayItemView:
  void HandleLocaleChange() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // UnifiedSystemTrayModel::Observer:
  void OnSystemTrayButtonSizeChanged(
      UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) override;

  // Reset the view by removing observer to |model_|.
  void Reset();

  // views::View:
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 private:
  friend class TimeTrayItemViewTest;

  scoped_refptr<UnifiedSystemTrayModel> model_;
  TimeView* time_view_ = nullptr;
  ScopedSessionObserver session_observer_;
  base::ScopedObservation<UnifiedSystemTrayModel,
                          UnifiedSystemTrayModel::Observer>
      system_tray_model_observation_{this};
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_TIME_TIME_TRAY_ITEM_VIEW_H_
