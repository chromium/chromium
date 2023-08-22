// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_MANUAL_TEST_HEARTBEAT_EVENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_MANUAL_TEST_HEARTBEAT_EVENT_H_

#include <memory>

#include "build/chromeos_buildflags.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace reporting {

// This class is only used for manual testing purpose. Do not depend on it in
// other parts of the production code.
class ManualTestHeartbeatEvent :
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Used to listen for user login.
    public policy::ManagedSessionService::Observer,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    public KeyedService {
 public:
  ManualTestHeartbeatEvent();
  ~ManualTestHeartbeatEvent() override;

  // KeyedService
  void Shutdown() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ManagedSessionService::Observer:
  void OnLogin(Profile* profile) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  // Starts a self-managed ReportQueueManualTestContext running on its own
  // SequencedTaskRunner. Will upload ten records to the HEARTBEAT_EVENTS
  // Destination and delete itself.
  void StartHeartbeatEvent() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Object that monitors managed session related events used by reporting
  // services.
  std::unique_ptr<policy::ManagedSessionService> managed_session_service_;
  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_MANUAL_TEST_HEARTBEAT_EVENT_H_
