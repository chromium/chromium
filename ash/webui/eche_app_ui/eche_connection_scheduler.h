// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_SCHEDULER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_SCHEDULER_H_

namespace ash {
namespace eche_app {

// Responsible for requesting connection from the local device
// (e.g. this chromebook) to the user's phone. Will also attempt to connect
// whenever possible and retries upon error with exponential backoff.
class EcheConnectionScheduler {
 public:
  EcheConnectionScheduler(const EcheConnectionScheduler&) = delete;
  EcheConnectionScheduler& operator=(const EcheConnectionScheduler&) = delete;
  virtual ~EcheConnectionScheduler() = default;

  // Attempts a connection immediately, will be exponentially backed-off upon
  // failing to establish a connection.
  virtual void ScheduleConnectionNow() = 0;

  // Invalidate all pending backoff attempts and disconnects the current
  // connection attempt.
  virtual void DisconnectAndClearBackoffAttempts() = 0;

 protected:
  EcheConnectionScheduler() = default;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_CONNECTION_SCHEDULER_H_
