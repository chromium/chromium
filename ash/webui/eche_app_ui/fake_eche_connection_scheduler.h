// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_FAKE_ECHE_CONNECTION_SCHEDULER_H_
#define ASH_WEBUI_ECHE_APP_UI_FAKE_ECHE_CONNECTION_SCHEDULER_H_

#include <stddef.h>

#include "ash/webui/eche_app_ui/eche_connection_scheduler.h"

namespace ash {
namespace eche_app {

class FakeEcheConnectionScheduler : public EcheConnectionScheduler {
 public:
  FakeEcheConnectionScheduler();
  ~FakeEcheConnectionScheduler() override;

  size_t num_schedule_connection_now_calls() const {
    return num_schedule_connection_now_calls_;
  }

  size_t num_disconnect_calls() const { return num_disconnect_calls_; }

 private:
  // EcheConnectionScheduler:
  void ScheduleConnectionNow() override;
  void DisconnectAndClearBackoffAttempts() override;

  size_t num_schedule_connection_now_calls_ = 0u;
  size_t num_disconnect_calls_ = 0u;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_FAKE_ECHE_CONNECTION_SCHEDULER_H_
