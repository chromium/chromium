// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_MANUAL_TEST_HEARTBEAT_EVENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_MANUAL_TEST_HEARTBEAT_EVENT_H_

#include "base/feature_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace reporting {

// This class is only used for manual testing purpose. Do not depend on it in
// other parts of the production code.
class ManualTestHeartbeatEvent : public KeyedService {
 public:
  ManualTestHeartbeatEvent();
  ~ManualTestHeartbeatEvent() override;

  // KeyedService
  void Shutdown() override;

 private:
  // Starts a self-managed ReportQueueManualTestContext running on its own
  // SequencedTaskRunner. Will upload ten records to the HEARTBEAT_EVENTS
  // Destination and delete itself.
  void StartHeartbeatEvent() const;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_MANUAL_TEST_HEARTBEAT_EVENT_H_
