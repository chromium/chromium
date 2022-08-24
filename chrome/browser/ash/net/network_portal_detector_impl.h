// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_DETECTOR_IMPL_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_DETECTOR_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/captive_portal/core/captive_portal_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace ash {

class NetworkState;

// This class handles all notifications about network changes from
// NetworkStateHandler and delegates portal detection for the default
// network to captive_portal::CaptivePortalService.
class NetworkPortalDetectorImpl : public NetworkPortalDetector,
                                  public NetworkStateHandlerObserver,
                                  public content::NotificationObserver {
 public:
  explicit NetworkPortalDetectorImpl(
      network::mojom::URLLoaderFactory* loader_factory_for_testing = nullptr);

  NetworkPortalDetectorImpl(const NetworkPortalDetectorImpl&) = delete;
  NetworkPortalDetectorImpl& operator=(const NetworkPortalDetectorImpl&) =
      delete;

  ~NetworkPortalDetectorImpl() override;

  // NetworkPortalDetector implementation:
  void AddObserver(Observer* observer) override;
  void AddAndFireObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  CaptivePortalStatus GetCaptivePortalStatus() override;
  bool IsEnabled() override;
  void Enable() override;
  void StartPortalDetection() override;

 private:
  friend class NetworkPortalDetectorImplTest;
  friend class NetworkPortalDetectorImplBrowserTest;

  enum State {
    // No portal check is running.
    STATE_IDLE = 0,
    // Waiting for portal check.
    STATE_PORTAL_CHECK_PENDING,
    // Portal check is in progress.
    STATE_CHECKING_FOR_PORTAL,
  };

  // Starts detection process.
  void StartDetection();

  // Stops whole detection process.
  void StopDetection();

  // Stops and restarts the detection process.
  void RetryDetection();

  // Initiates Captive Portal detection attempt after |delay|.
  void ScheduleAttempt(const base::TimeDelta& delay = base::TimeDelta());

  // Starts detection attempt.
  void StartAttempt();

  // Called when portal check is timed out. Cancels portal check and calls
  // OnPortalDetectionCompleted() with RESULT_NO_RESPONSE as a result.
  void OnAttemptTimeout();

  // Called by CaptivePortalDetector when detection attempt completes.
  void OnAttemptCompleted(
      const captive_portal::CaptivePortalDetector::Results& results);

  // NetworkStateHandlerObserver implementation:
  void OnShuttingDown() override;
  void PortalStateChanged(const NetworkState* default_network,
                          NetworkState::PortalState portal_state) override;

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void DetectionCompleted(const NetworkState* network,
                          const CaptivePortalStatus& results);

  void ResetCountersAndSendMetrics();

  // Returns true if attempt timeout callback isn't fired or
  // cancelled.
  bool AttemptTimeoutIsCancelledForTesting() const;

  State state() const { return state_; }

  bool is_idle() const { return state_ == STATE_IDLE; }
  bool is_portal_check_pending() const {
    return state_ == STATE_PORTAL_CHECK_PENDING;
  }
  bool is_checking_for_portal() const {
    return state_ == STATE_CHECKING_FOR_PORTAL;
  }

  int captive_portal_detector_run_count_for_testing() const {
    return captive_portal_detector_run_count_;
  }

  void set_attempt_delay_for_testing(base::TimeDelta delay) {
    attempt_delay_for_testing_ = delay;
  }

  void set_attempt_timeout_for_testing(base::TimeDelta timeout) {
    attempt_timeout_ = timeout;
  }

  const base::TimeDelta& next_attempt_delay_for_testing() const {
    return next_attempt_delay_;
  }

  const std::string& default_network_id_for_testing() const {
    return default_network_id_;
  }

  int response_code_for_testing() const { return response_code_for_testing_; }

  // Unique identifier of the default network.
  std::string default_network_id_;

  CaptivePortalStatus default_portal_status_ = CAPTIVE_PORTAL_STATUS_UNKNOWN;
  int response_code_for_testing_ = -1;

  State state_ = STATE_IDLE;
  base::ObserverList<Observer>::Unchecked observers_;

  base::CancelableOnceClosure attempt_task_;
  base::CancelableOnceClosure attempt_timeout_task_;

  // Reference to a SharedURLLoaderFactory used to detect portals.
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Detector for checking default network for a portal state.
  std::unique_ptr<captive_portal::CaptivePortalDetector>
      captive_portal_detector_;

  // True if the NetworkPortalDetector is enabled.
  bool enabled_ = false;

  // Delay before next portal detection.
  base::TimeDelta next_attempt_delay_;

  // Delay before next portal detection for testing.
  absl::optional<base::TimeDelta> attempt_delay_for_testing_;

  // Timeout before attempt is timed out.
  base::TimeDelta attempt_timeout_;

  // Last received result from captive portal detector.
  CaptivePortalStatus last_detection_status_ = CAPTIVE_PORTAL_STATUS_UNKNOWN;

  // Number of detection attempts with same result in a row.
  int same_detection_result_count_ = 0;

  // Number of detection attempts.
  int captive_portal_detector_run_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  content::NotificationRegistrar registrar_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<NetworkPortalDetectorImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_DETECTOR_IMPL_H_
