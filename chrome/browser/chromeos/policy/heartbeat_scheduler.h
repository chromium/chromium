// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_HEARTBEAT_SCHEDULER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_HEARTBEAT_SCHEDULER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace base {
class SequencedTaskRunner;
}

namespace gcm {
class GCMDriver;
}

namespace policy {

class HeartbeatRegistrationHelper;

// Class responsible for periodically sending heartbeats to the policy service
// for monitoring device connectivity.
class HeartbeatScheduler : public gcm::GCMAppHandler,
                           gcm::GCMConnectionObserver {
 public:
  // Default interval for how often we send up a heartbeat.
  static const base::TimeDelta kDefaultHeartbeatInterval;

  // UMA histogram name.
  static const char* const kHeartbeatSignalHistogram;

  // Constructor. |cloud_policy_client| will be used to send registered GCM id
  // to DM server, and can be null. |driver| can be null for tests.
  HeartbeatScheduler(
      gcm::GCMDriver* driver,
      policy::CloudPolicyClient* cloud_policy_client,
      const std::string& enrollment_domain,
      const std::string& device_id,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ~HeartbeatScheduler() override;

  // Returns the time of the last heartbeat, or Time(0) if no heartbeat
  // has ever happened.
  base::Time last_heartbeat() const { return last_heartbeat_; }

  // GCMAppHandler overrides.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(const std::string& app_id,
                   const gcm::GCMClient::SendErrorDetails& details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

  // GCMConnectionObserver overrides.
  void OnConnected(const net::IPEndPoint&) override;

 private:
  // Callback invoked periodically to send a heartbeat to the policy service.
  void SendHeartbeat();

  // Invoked after GCM registration has successfully completed.
  void OnRegistrationComplete(const std::string& registration_id);

  // Invoked after a heartbeat has been sent to the server.
  void OnHeartbeatSent(const std::string& message_id,
                       gcm::GCMClient::Result result);

  // Invoked after a upstream notification sign up message has been sent.
  void OnUpstreamNotificationSent(const std::string& message_id,
                                  gcm::GCMClient::Result result);

  // Helper method that figures out when the next heartbeat should
  // be scheduled.
  void ScheduleNextHeartbeat();

  // Updates the heartbeat-enabled status and frequency from settings and
  // schedules the next heartbeat.
  void RefreshHeartbeatSettings();

  // Ensures that the passed interval is within a valid range (not too large or
  // too small).
  base::TimeDelta EnsureValidHeartbeatInterval(const base::TimeDelta& interval);

  // Shuts down our GCM connection (called when heartbeats are disabled).
  void ShutdownGCM();

  // Callback for the GCM id update request.
  void OnGcmIdUpdateRequestSent(bool status);

  // Helper function to signup for upstream notification.
  void SignUpUpstreamNotification();

  // TaskRunner used for scheduling heartbeats.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The domain that this device is enrolled to.
  const std::string enrollment_domain_;

  // The device_id for this device - sent up with the enrollment domain with
  // heartbeats to identify the device to the server.
  const std::string device_id_;

  // True if heartbeats are enabled. Kept cached in this object because
  // CrosSettings can switch to an untrusted state temporarily, and we want
  // to use the last-known trusted values.
  bool heartbeat_enabled_;

  // Cached copy of the current heartbeat interval, in milliseconds.
  base::TimeDelta heartbeat_interval_;

  // Observers to changes in the heartbeat settings.
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      heartbeat_frequency_observer_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      heartbeat_enabled_observer_;

  // The time the last heartbeat was sent.
  base::Time last_heartbeat_;

  // Callback invoked via a delay to send a heartbeat.
  base::CancelableClosure heartbeat_callback_;

  policy::CloudPolicyClient* cloud_policy_client_;

  // The GCMDriver used to send heartbeat messages.
  gcm::GCMDriver* const gcm_driver_;

  // The GCM registration ID - if empty, we are not registered yet.
  std::string registration_id_;

  // If true, we are already registered with GCM and should unregister when
  // destroyed.
  bool registered_app_handler_ = false;

  // Helper class to manage registering with the GCM server, including
  // retries, etc.
  std::unique_ptr<HeartbeatRegistrationHelper> registration_helper_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<HeartbeatScheduler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HeartbeatScheduler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_HEARTBEAT_SCHEDULER_H_
