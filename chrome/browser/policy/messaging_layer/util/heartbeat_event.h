// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_HEARTBEAT_EVENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_HEARTBEAT_EVENT_H_

#include "base/feature_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace reporting {

class HeartbeatEvent : public KeyedService {
 public:
  HeartbeatEvent();
  ~HeartbeatEvent() override;

  // KeyedService
  void Shutdown() override;

 private:

  // Starts a self-managed ReportQueueManualTestContext running on its own
  // SequencedTaskRunner. Will upload ten records to the HEARTBEAT_EVENTS
  // Destination and delete itself.
  void StartHeartbeatEvent() const;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_HEARTBEAT_EVENT_H_
