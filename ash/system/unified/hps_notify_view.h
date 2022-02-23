// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_
#define ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/hps_notify_controller.h"
#include "base/scoped_observation.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// The icon in the system tray notifying a user that a second person has been
// detected looking over their shoulder.
class ASH_EXPORT HpsNotifyView : public TrayItemView,
                                 public SessionObserver,
                                 public HpsNotifyController::Observer {
 public:
  explicit HpsNotifyView(Shelf* shelf);
  HpsNotifyView(const HpsNotifyView&) = delete;
  HpsNotifyView& operator=(const HpsNotifyView&) = delete;
  ~HpsNotifyView() override;

  // views::TrayItemView:
  const char* GetClassName() const override;
  void HandleLocaleChange() override;
  void OnThemeChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // HpsNotifyController::Observer:
  void OnSnoopingStatusChanged(bool snooper) override;
  void OnHpsNotifyControllerDestroyed() override;

 private:
  // Updates the system tray icon to use the color corresponding to the current
  // session state (e.g. darker during OOBE).
  void UpdateIconColor(session_manager::SessionState session_state);

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  base::ScopedObservation<HpsNotifyController, HpsNotifyController::Observer>
      controller_observation_{this};

  // Must be last.
  base::WeakPtrFactory<HpsNotifyView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_
