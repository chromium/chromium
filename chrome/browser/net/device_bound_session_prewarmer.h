// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DEVICE_BOUND_SESSION_PREWARMER_H_
#define CHROME_BROWSER_NET_DEVICE_BOUND_SESSION_PREWARMER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/wall_clock_timer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "net/device_bound_sessions/session_access.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"
#include "url/gurl.h"

// Helper class to proactively refresh (pre-warm) Device Bound Session
// Credentials (DBSC) cookies for a specific HTTPS URL.
//
// It triggers the pre-warming process when `Start()` is called.
// After the first trigger, it uses the Mojo service return value
// (`earliest_next_refresh_time`) to schedule subsequent pre-warming.
//
// It observes session access events via `DeviceBoundSessionAccessObserver`
// and schedules a new pre-warming when a new bound session is created.
//
// If `earliest_next_refresh_time` is null it will not schedule a new
// pre-warming, unless there are transient errors, in which case it will use
// the minimum interval.
//
// If `earliest_next_refresh_time` is in the past or shorter than the minimum
// interval, it will schedule the next pre-warming at the minimum interval to
// avoid infinite loops or excessive requests.
//
// Scheduling uses wall-clock time, so a pre-warm due during suspend runs on
// resume. An unreachable pre-warm is retried as soon as connectivity returns
// (including network switches), or right away if connectivity returned while
// it was in flight.
class DeviceBoundSessionPrewarmer
    : public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public network::mojom::DeviceBoundSessionAccessObserver {
 public:
  // What caused a pre-warm to be sent. Recorded to
  // `Net.DeviceBoundSessions.Prewarm.Trigger` and its per-outcome variants.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(PrewarmTrigger)
  enum class PrewarmTrigger {
    // `Start()` was called.
    kStart = 0,
    // The next refresh time returned by the previous pre-warm was reached.
    kScheduledRefresh = 1,
    // The previous pre-warm had a transient error.
    kTransientErrorRetry = 2,
    // Connectivity returned after the previous pre-warm was unreachable.
    kReconnect = 3,
    // Connectivity returned while the previous pre-warm was in flight, and it
    // then came back unreachable.
    kReconnectDuringPrewarm = 4,
    // A session including `prewarm_url()` was created.
    kSessionCreated = 5,
    // The network service disconnected, e.g. because it crashed.
    kNetworkServiceDisconnected = 6,
    // The previous pre-warm couldn't be sent because the network service was
    // unavailable.
    kNetworkServiceUnavailableRetry = 7,
    kMaxValue = kNetworkServiceUnavailableRetry,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:DeviceBoundSessionPrewarmTrigger)

  // A callback to retrieve the DeviceBoundSessionManager pointer dynamically.
  // This handles the case where the network service crashes and restarts,
  // providing a new pointer when necessary.
  using SessionManagerProvider =
      base::RepeatingCallback<network::mojom::DeviceBoundSessionManager*()>;

  // `prewarm_url` must be a valid HTTPS URL.
  // `network_connection_tracker` must be non-null and must outlive `this`.
  DeviceBoundSessionPrewarmer(
      GURL prewarm_url,
      SessionManagerProvider session_manager_provider,
      network::NetworkConnectionTracker* network_connection_tracker);
  DeviceBoundSessionPrewarmer(const DeviceBoundSessionPrewarmer&) = delete;
  DeviceBoundSessionPrewarmer& operator=(const DeviceBoundSessionPrewarmer&) =
      delete;
  ~DeviceBoundSessionPrewarmer() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Starts the pre-warmer. The first execution will be immediate.
  // If the pre-warmer is already running, it will be stopped and restarted.
  void Start(bool is_startup_prewarm);

  // Stops the pre-warmer.
  void Stop();

  const GURL& prewarm_url() const { return prewarm_url_; }

  // network::mojom::DeviceBoundSessionAccessObserver:
  void OnDeviceBoundSessionAccessed(
      const net::device_bound_sessions::SessionAccess& access) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::DeviceBoundSessionAccessObserver>
          observer) override;

 private:
  // Calls the Mojo service if available or retries again after a timeout.
  // `trigger` is what caused this pre-warm.
  void DoPrewarm(PrewarmTrigger trigger);

  // Schedules `DoPrewarm(trigger)` after `delay`, replacing any pre-warm
  // already scheduled.
  void SchedulePrewarm(base::TimeDelta delay, PrewarmTrigger trigger);

  // Ensures that `receiver_` is bound and observing session accesses for
  // `prewarm_url_`.
  void EnsureObserverBound(
      network::mojom::DeviceBoundSessionManager* session_manager);

  // Called when the Mojo observer pipe disconnects (e.g. network service
  // crash).
  void OnObserverDisconnected();

  // Callback from network service containing prewarming results.
  // `trigger` is what caused the pre-warm, and `prewarm_timer` was started
  // when it was issued.
  void OnPrewarmComplete(
      PrewarmTrigger trigger,
      base::ElapsedTimer prewarm_timer,
      const std::vector<net::device_bound_sessions::RefreshResult>& results,
      std::optional<base::Time> earliest_next_refresh_time);

  // Returns true if the result is a transient error.
  static bool IsTransientError(
      net::device_bound_sessions::RefreshResult result);

  const GURL prewarm_url_;
  const SessionManagerProvider session_manager_provider_;
  base::WallClockTimer timer_;

  // The last pre-warm was unreachable; regaining connectivity retries it.
  bool retry_on_reconnect_ = false;

  // Set when connectivity returns. If the pre-warm in flight then comes back
  // unreachable, it may have been sent on the old network, so it's retried
  // immediately instead of after `kMinPrewarmInterval`.
  bool retry_on_unreachable_ = false;

  // Whether the current pre-warming is the startup pre-warming (from Start())
  // or a subsequent scheduled pre-warming.
  bool is_startup_prewarm_ = true;

  base::ScopedObservation<
      network::NetworkConnectionTracker,
      network::NetworkConnectionTracker::NetworkConnectionObserver>
      network_connection_observer_{this};
  // Seeded from `IsOffline()`, so also `true` while the connection type is
  // unknown.
  bool is_offline_ = true;

  mojo::Receiver<network::mojom::DeviceBoundSessionAccessObserver> receiver_{
      this};

  base::WeakPtrFactory<DeviceBoundSessionPrewarmer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NET_DEVICE_BOUND_SESSION_PREWARMER_H_
