// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_
#define ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// The current HPS notify state in the system tray.
class HpsNotifyView : public TrayItemView,
                      public SessionObserver,
                      public chromeos::HpsDBusClient::Observer {
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

  // chromeos::HpsDBusClient::Observer:
  void OnHpsNotifyChanged(bool state) override;

 private:
  // Updates the system tray icon to use the color corresponding to the current
  // session state (e.g. darker during OOBE).
  void UpdateIconColor(session_manager::SessionState session_state);

  // Shows or hides the system tray icon as appropriate given the signal and
  // session state. Also updates the current cached states.
  void UpdateIconVisibility(bool is_oobe, bool hps_state);

  // A callback to set our initial state by polling HPS. If we have received a
  // signal before the callback is executed, this becomes a no-op.
  void OnHpsPollResult(absl::optional<bool> result);

  // Used to track whether a signal should actually trigger a visibility change.
  bool hps_state_;
  bool is_oobe_;

  // Used to avoid a race between polling and being signaled by the HPS service.
  bool first_signal_received_;

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_;
  base::ScopedObservation<chromeos::HpsDBusClient,
                          chromeos::HpsDBusClient::Observer>
      hps_dbus_observation_;

  // Must be last.
  base::WeakPtrFactory<HpsNotifyView> weak_ptr_factory_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_HPS_NOTIFY_VIEW_H_
