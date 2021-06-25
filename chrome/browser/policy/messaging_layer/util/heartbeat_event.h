// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_HEARTBEAT_EVENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_HEARTBEAT_EVENT_H_

#include "base/feature_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"

namespace reporting {

class HeartbeatEvent : public KeyedService {
 public:
  class CloudPolicyServiceObserver
      : public policy::CloudPolicyService::Observer {
   public:
    explicit CloudPolicyServiceObserver(
        base::RepeatingCallback<void()> start_heartbeat_event);
    ~CloudPolicyServiceObserver() override;

    void OnCloudPolicyServiceInitializationCompleted() override;

   private:
    // We take a RepeatingCallback instead of a OnceCallback in case there is
    // are multiple calls before we can unsubscribe from the observer list.
    base::RepeatingCallback<void()> start_heartbeat_event_;
  };

  explicit HeartbeatEvent(policy::CloudPolicyManager* manager);
  ~HeartbeatEvent() override;

  // KeyedService
  void Shutdown() override;

 private:
  // Passed to the CloudPolicyServiceObserver as a RepeatingCallback calls
  // Shutdown to ensure the observer is removed from the observer list and then
  // destroys |cloud_policy_service_observer_| before calling
  // StartHeartbeatEvent.
  void HandleNotification();

  // Starts a self-managed ReportQueueManualTestContext running on its own
  // SequencedTaskRunner. Will upload ten records to the HEARTBEAT_EVENTS
  // Destination and delete itself.
  void StartHeartbeatEvent() const;

  // Holds the CloudPolicyService which knows when CloudPolicyClient is
  // available.
  policy::CloudPolicyManager* manager_;

  // Observer for CloudPolicyService, only used if it has not been initialized
  // by the time we are called.
  std::unique_ptr<CloudPolicyServiceObserver> cloud_policy_service_observer_;

  // Blocks additional calls on HandleNotification.
  std::atomic<bool> notified_{false};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_HEARTBEAT_EVENT_H_
