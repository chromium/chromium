// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AVAILABILITY_AVAILABILITY_PROBER_H_
#define CHROME_BROWSER_AVAILABILITY_AVAILABILITY_PROBER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

class PrefRegistrySimple;
class PrefService;

namespace network {
class NetworkConnectionTracker;
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

typedef base::RepeatingCallback<void(bool)>
    AvailabilityProberOnCompleteCallback;

// This class is a utility to probe a given URL with a given set of behaviors.
// This can be used for determining whether a specific network resource is
// available or accessible by Chrome.
// This class may live on either UI or IO thread but should remain on the thread
// that it was created on.
class AvailabilityProber
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  class Delegate {
   public:
    // This check is called before each probe is sent on the network. This can
    // be used to check for permitting feature state or other runtime checks. If
    // the delegate returns false, no more probes would be attempted until there
    // is a change in the network or |SendNowIfInactive| is called.
    virtual bool ShouldSendNextProbe() = 0;

    // Allows the delegate to decide what responses mean success. If the
    // delegate returns true, no more probes would be attempted until there is a
    // change in the network or |SendNowIfInactive| is called.
    virtual bool IsResponseSuccess(net::Error net_error,
                                   const network::mojom::URLResponseHead* head,
                                   std::unique_ptr<std::string> body) = 0;
  };

  // Callers who wish to use this class should add a value to this enum. This
  // enum is mapped to a string value which is then used in histograms and
  // prefs.
  enum class ClientName {
    kLitepages = 0,

    kLitepagesOriginCheck = 1,

    kMaxValue = kLitepagesOriginCheck,
  };

  // This enum describes the different algorithms that can be used to calculate
  // a time delta between probe events like retries or timeout ttl.
  enum class Backoff {
    // Use the same time delta for each event.
    kLinear,

    // Use an exponentially increasing time delta, base 2.
    kExponential,
  };

  struct RetryPolicy {
    RetryPolicy();
    RetryPolicy(const RetryPolicy& other);
    ~RetryPolicy();

    // The maximum number of retries (not including the original probe) to
    // attempt.
    size_t max_retries = 3;

    // How to compute the time interval between successive retries.
    Backoff backoff = Backoff::kLinear;

    // Time between probes as the base value. For example, given |backoff|:
    //   LINEAR: |base_interval| between the end of last probe and start of next
    //           probe.
    //   EXPONENTIAL: (|base_interval| * 2 ^ |successive_retry_count_|) between
    //                the end of last retry and start of next retry.
    base::TimeDelta base_interval = base::TimeDelta();

    // If true, this attaches a random GUID query param to the URL of every
    // probe, including the first probe.
    bool use_random_urls = false;
  };

  struct TimeoutPolicy {
    TimeoutPolicy();
    TimeoutPolicy(const TimeoutPolicy& other);
    ~TimeoutPolicy();

    // How to compute the TTL of probes.
    Backoff backoff = Backoff::kLinear;

    // The TTL base value. For example,
    //   LINEAR: Each probe times out in |base_timeout|.
    //   EXPONENTIAL: Each probe times out in
    //                (|base_timeout| * 2 ^ |successive_timeout_count_|).
    base::TimeDelta base_timeout = base::TimeDelta::FromSeconds(60);
  };

  enum class HttpMethod {
    kGet,
    kHead,
  };

  AvailabilityProber(
      Delegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      ClientName name,
      const GURL& url,
      HttpMethod http_method,
      const net::HttpRequestHeaders headers,
      const RetryPolicy& retry_policy,
      const TimeoutPolicy& timeout_policy,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const size_t max_cache_entries,
      base::TimeDelta revalidate_cache_after);
  ~AvailabilityProber() override;

  // Registers the prefs used in this class.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Clears the prefs used in this class.
  static void ClearData(PrefService* pref_service);

  // Sends a probe now if the prober is currently inactive. If the probe is
  // active (i.e.: there are probes in flight), this is a no-op. If
  // |send_only_in_foreground| is set, the probe will only be sent when the app
  // is in the foreground (work on Android only).
  void SendNowIfInactive(bool send_only_in_foreground);

  // Calls |SendNowIfInactive| immediately with |send_only_in_foreground| and
  // repeats every |interval|. Calling this multiple times with different
  // |interval| will change the interval to the most recently provided value.
  // Only stops when |this| is deleted.
  void RepeatedlyProbe(base::TimeDelta interval, bool send_only_in_foreground);

  // Returns the successfulness of the last probe, if there was one. If the last
  // probe status was cached and needs to be revalidated, this may activate the
  // prober.
  base::Optional<bool> LastProbeWasSuccessful();

  // True if probes are being attempted, including retries.
  bool is_active() const { return time_when_set_active_.has_value(); }

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Sets a repeating callback to notify the completion of a probe and whether
  // it was successful. It is safe to delete |this| during the callback.
  void SetOnCompleteCallback(AvailabilityProberOnCompleteCallback callback);

  // Called when some other network request to the same endpoint has failed and
  // the probe should be reattempted. Also records this event as a probe
  // failure.
  void ReportExternalFailureAndRetry();

 protected:
  // Exposes |tick_clock| and |clock| for testing.
  AvailabilityProber(
      Delegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      ClientName name,
      const GURL& url,
      HttpMethod http_method,
      const net::HttpRequestHeaders headers,
      const RetryPolicy& retry_policy,
      const TimeoutPolicy& timeout_policy,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const size_t max_cache_entries,
      base::TimeDelta revalidate_cache_after,
      const base::TickClock* tick_clock,
      const base::Clock* clock);

 private:
  void ResetState();
  void CreateAndStartURLLoader();
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);
  void ProcessProbeTimeout();
  void ProcessProbeFailure();
  void ProcessProbeSuccess();
  void AddSelfAsNetworkConnectionObserver(
      network::NetworkConnectionTracker* network_connection_tracker);
  void RecordProbeResult(bool success);
  std::string GetCacheKeyForCurrentNetwork() const;
  std::string AppendNameToHistogram(const std::string& histogram) const;
  void RunCallback(bool success);
#if defined(OS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState new_state);
#endif

  // This is called whenever the prober goes inactive. This is caused whenever
  // the probe succeeds, fails and there are no more retries, or the delegate
  // stops the probing.
  void OnProbingEnd();

  // Must outlive |this|.
  Delegate* delegate_;

  // The name given to this prober instance, used in metrics, prefs, and
  // traffic annotations.
  const std::string name_;

  // The pref key for used to recording |cached_probe_results_| to disk.
  const std::string pref_key_;

  // The URL that will be probed.
  const GURL url_;

  // The HTTP method used for probing.
  const HttpMethod http_method_;

  // Additional headers to send on every probe. These are subject to CORS
  // checks.
  const net::HttpRequestHeaders headers_;

  // The retry policy to use in this prober.
  const RetryPolicy retry_policy_;

  // The timeout policy to use in this prober.
  const TimeoutPolicy timeout_policy_;

  // The maximum allowable size of |cached_probe_results_|.
  const size_t max_cache_entries_;

  // How long to allow a cached entry to be valid until it is revalidated in the
  // background.
  const base::TimeDelta revalidate_cache_after_;

  // The traffic annotation to use for creating |url_loader_|.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // The number of retries that have been attempted. This count does not include
  // the original probe.
  size_t successive_retry_count_;

  // The number of timeouts that have occurred.
  size_t successive_timeout_count_;

  // If a retry is being attempted, this will be running until the next attempt.
  std::unique_ptr<base::OneShotTimer> retry_timer_;

  // If a probe is being attempted, this will be running until the TTL.
  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  // If we are repeatedly probing, this will be running.
  std::unique_ptr<base::RepeatingTimer> repeating_timer_;

  // Caches past probe results in a mapping of one tuple to another:
  //   (network_id, url_) -> (last_probe_status, last_modification_time).
  // No more than |max_cache_entries_| will be kept in this dictionary.
  // |cached_probe_results_| may differ from what is on disk in the event
  // browsing history is cleared during the limetime of |this|.
  std::unique_ptr<base::DictionaryValue> cached_probe_results_;

  // The tick clock used within this class.
  const base::TickClock* tick_clock_;

  // The time clock used within this class.
  const base::Clock* clock_;

  // Remembers the last time the prober became active.
  base::Optional<base::Time> time_when_set_active_;

  // This reference is kept around for unregistering |this| as an observer on
  // any thread.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Reference for saving |cached_probe_results_| to prefs.
  PrefService* pref_service_;

  // Used for setting up the |url_loader_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URLLoader used for the probe. Expected to be non-null iff
  // |is_active()|.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

#if defined(OS_ANDROID)
  // Set if |SendInForegroundIfInactive| is called while app is in the
  // background and listens until app comes to the foreground, then resets.
  std::unique_ptr<base::android::ApplicationStatusListener>
      application_status_listener_;
#endif

  // An optional callback to notify of a completed probe. This callback passes a
  // bool to indicate success of the completed probe.
  AvailabilityProberOnCompleteCallback on_complete_callback_;

  // This is set after a call to |ReportExternalFailureAndRetry| and cleared
  // when the next probe completes. This state is kept for histogram recording
  // of success after a reported failure.
  bool reported_external_failure_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AvailabilityProber> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AvailabilityProber);
};

#endif  // CHROME_BROWSER_AVAILABILITY_AVAILABILITY_PROBER_H_
