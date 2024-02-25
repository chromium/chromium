// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_SCHEDULER_IMPL_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_SCHEDULER_IMPL_H_

#include "ash/webui/eche_app_ui/eche_connection_scheduler.h"
#include "ash/webui/eche_app_ui/feature_status.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "net/base/backoff_entry.h"

namespace ash {
namespace eche_app {

using ConnectionStatus = secure_channel::ConnectionManager::Status;

// EcheConnectionScheduler implementation that schedules calls to
// ConnectionManager in order to establish a connection to the user's phone.
class EcheConnectionSchedulerImpl
    : public EcheConnectionScheduler,
      public FeatureStatusProvider::Observer,
      public secure_channel::ConnectionManager::Observer {
 public:
  EcheConnectionSchedulerImpl(
      secure_channel::ConnectionManager* connection_manager,
      FeatureStatusProvider* feature_status_provider);
  ~EcheConnectionSchedulerImpl() override;

  void ScheduleConnectionNow() override;

  void DisconnectAndClearBackoffAttempts() override;

 private:
  friend class EcheConnectionSchedulerImplTest;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  void ScheduleConnectionIfNeeded();

  void ClearBackoffAttempts();

  // Test only functions.
  base::TimeDelta GetCurrentBackoffDelayTimeForTesting();
  int GetBackoffFailureCountForTesting();

  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  raw_ptr<FeatureStatusProvider> feature_status_provider_;
  // Provides us the backoff timers for RequestConnection().
  net::BackoffEntry retry_backoff_;
  FeatureStatus current_feature_status_;
  ConnectionStatus current_connection_status_;
  base::WeakPtrFactory<EcheConnectionSchedulerImpl> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_SCHEDULER_IMPL_H_
