// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DEVICE_BOUND_SESSION_PREWARMER_H_
#define CHROME_BROWSER_NET_DEVICE_BOUND_SESSION_PREWARMER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/device_bound_sessions/refresh_result.h"
#include "net/device_bound_sessions/session_access.h"
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
class DeviceBoundSessionPrewarmer
    : public network::mojom::DeviceBoundSessionAccessObserver {
 public:
  // A callback to retrieve the DeviceBoundSessionManager pointer dynamically.
  // This handles the case where the network service crashes and restarts,
  // providing a new pointer when necessary.
  using SessionManagerProvider =
      base::RepeatingCallback<network::mojom::DeviceBoundSessionManager*()>;

  // `prewarm_url` must be a valid HTTPS URL.
  explicit DeviceBoundSessionPrewarmer(
      GURL prewarm_url,
      SessionManagerProvider session_manager_provider);
  DeviceBoundSessionPrewarmer(const DeviceBoundSessionPrewarmer&) = delete;
  DeviceBoundSessionPrewarmer& operator=(const DeviceBoundSessionPrewarmer&) =
      delete;
  ~DeviceBoundSessionPrewarmer() override;

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
  void DoPrewarm();

  // Ensures that `receiver_` is bound and observing session accesses for
  // `prewarm_url_`.
  void EnsureObserverBound(
      network::mojom::DeviceBoundSessionManager* session_manager);

  // Called when the Mojo observer pipe disconnects (e.g. network service
  // crash).
  void OnObserverDisconnected();

  // Callback from network service containing prewarming results.
  void OnPrewarmComplete(
      const std::vector<net::device_bound_sessions::RefreshResult>& results,
      std::optional<base::Time> earliest_next_refresh_time);

  // Returns true if the result is a transient error.
  static bool IsTransientError(
      net::device_bound_sessions::RefreshResult result);

  const GURL prewarm_url_;
  const SessionManagerProvider session_manager_provider_;
  base::OneShotTimer timer_;

  // Whether the current pre-warming is the startup pre-warming (from Start())
  // or a subsequent scheduled pre-warming.
  bool is_startup_prewarm_ = true;

  mojo::Receiver<network::mojom::DeviceBoundSessionAccessObserver> receiver_{
      this};
  base::WeakPtrFactory<DeviceBoundSessionPrewarmer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NET_DEVICE_BOUND_SESSION_PREWARMER_H_
