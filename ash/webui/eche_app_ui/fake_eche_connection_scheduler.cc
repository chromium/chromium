// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/fake_eche_connection_scheduler.h"

namespace ash {
namespace eche_app {

FakeEcheConnectionScheduler::FakeEcheConnectionScheduler() = default;
FakeEcheConnectionScheduler::~FakeEcheConnectionScheduler() = default;

void FakeEcheConnectionScheduler::ScheduleConnectionNow() {
  ++num_schedule_connection_now_calls_;
}

void FakeEcheConnectionScheduler::DisconnectAndClearBackoffAttempts() {
  ++num_disconnect_calls_;
}

}  // namespace eche_app
}  // namespace ash
