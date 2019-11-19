// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/containers/small_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/common/media_router/discovery/media_sink_service_base.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_socket.h"
#include "net/base/backoff_entry.h"

namespace cast_channel {
class CastSocketService;
}

namespace media_router {

// Discovers and manages Cast MediaSinks using CastSocketService. This class
// also observes DialMediaSinkServiceImpl for sinks to connect to (also known
// as dual discovery).
// This class may be created on any thread. All methods, unless otherwise noted,
// must be invoked on the SequencedTaskRunner given by |task_runner_|.
class CastMediaSinkServiceImpl : public MediaSinkServiceBase,
                                 public cast_channel::CastSocket::Observer,
                                 public DiscoveryNetworkMonitor::Observer,
                                 public MediaSinkServiceBase::Observer {
 public:
  using SinkSource = CastDeviceCountMetrics::SinkSource;

  // The max number of cast channel open failure for a DIAL-discovered sink
  // before we can say confidently that it is unlikely to be a Cast device.
  static constexpr int kMaxDialSinkFailureCount = 10;

  // Returns a Cast MediaSink ID from a DIAL MediaSink ID, and vice versa.
  static MediaSink::Id GetCastSinkIdFromDial(const MediaSink::Id& dial_sink_id);
  static MediaSink::Id GetDialSinkIdFromCast(const MediaSink::Id& cast_sink_id);

  // |callback|: Callback passed to MediaSinkServiceBase.
  // |observer|: Observer to invoke on sink updates. Can be nullptr.
  // |cast_socket_service|: CastSocketService to use to open Cast channels to
  // discovered devices.
  // |network_monitor|: DiscoveryNetworkMonitor to use to listen for network
  // changes.
  // |dial_media_sink_service|: DialMediaSinkServiceImpl for dual discovery.
  // |allow_all_ips|: If |true|, |this| will try to open channel to
  //     sinks on all IPs, and not just private IPs.
  CastMediaSinkServiceImpl(const OnSinksDiscoveredCallback& callback,
                           cast_channel::CastSocketService* cast_socket_service,
                           DiscoveryNetworkMonitor* network_monitor,
                           MediaSinkServiceBase* dial_media_sink_service,
                           bool allow_all_ips);
  ~CastMediaSinkServiceImpl() override;

  // Returns the SequencedTaskRunner that should be used to invoke methods on
  // this instance. Can be invoked on any thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  void SetClockForTest(base::Clock* clock);

  // Marked virtual for tests. Registers observers to listen for Cast devices
  // and network changes.
  virtual void Start();

  // Attempts to open cast channels for |cast_sinks|. To avoid spamming a device
  // when it comes online, a randomized delay is introduced before an attempt to
  // open channel is made.
  void OpenChannelsWithRandomizedDelay(
      const std::vector<MediaSinkInternal>& cast_sinks,
      SinkSource sink_source);

  // Attempts to open cast channels for |cast_sinks| without delay. This method
  // is called when a user gesture is detected. |kConnectionRetry| will be used
  // as the SinkSource.
  // |cast_sinks|: list of sinks found by current round of mDNS discovery.
  void OpenChannelsNow(const std::vector<MediaSinkInternal>& cast_sinks);

  // Called by CastMediaSinkService to set |allow_all_ips_|.
  void SetCastAllowAllIPs(bool allow_all_ips);

 private:
  friend class CastMediaSinkServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelOpenSucceeded);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestMultipleOnChannelOpenSucceeded);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestTimer);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOpenChannelNoRetry);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOpenChannelRetryOnce);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestOpenChannelFails);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestMultipleOpenChannels);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelOpenFailed);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelErrorMayRetryForConnectingChannel);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelErrorMayRetryForCastSink);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelErrorNoRetryForMissingSink);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnSinkAddedOrUpdated);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnDiscoveryComplete);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheSinksForKnownNetwork);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheContainsOnlyResolvedSinks);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheUpdatedOnChannelOpenFailed);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, UnknownNetworkNoCache);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheUpdatedForKnownNetwork);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheDialDiscoveredSinks);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           DualDiscoveryDoesntDuplicateCacheItems);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheSinksForDirectNetworkChange);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, OpenChannelsNow);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParametersWithFeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParameters);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParametersWithDefaultValue);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestCreateCastSocketOpenParams);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParametersWithFeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestInitParameters);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParametersWithDefaultValue);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnSinkAddedOrUpdatedSkipsIfNonCastDevice);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelErrorRetry);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           OpenChannelNewIPSameSink);

  // Holds Finch field trial parameters controlling Cast channel retry strategy.
  struct RetryParams {
    // Initial delay (in ms) once backoff starts.
    int initial_delay_in_milliseconds;

    // Max retry attempts allowed when opening a Cast socket.
    int max_retry_attempts;

    // Factor by which the delay will be multiplied on each subsequent failure.
    // This must be >= 1.0.
    double multiply_factor;

    RetryParams();
    ~RetryParams();

    bool Validate();

    static RetryParams GetFromFieldTrialParam();
  };

  // Holds Finch field trial parameters controlling Cast channel open.
  struct OpenParams {
    // Connect timeout value when opening a Cast socket.
    int connect_timeout_in_seconds;

    // Amount of idle time to wait before pinging the Cast device.
    int ping_interval_in_seconds;

    // Amount of idle time to wait before disconnecting.
    int liveness_timeout_in_seconds;

    // Dynamic time out delta for connect timeout and liveness timeout. If
    // previous channel open operation with opening parameters (liveness
    // timeout, connect timeout) fails, next channel open will have parameters
    // (liveness timeout + delta, connect timeout + delta).
    int dynamic_timeout_delta_in_seconds;

    OpenParams();
    ~OpenParams();

    bool Validate();

    static OpenParams GetFromFieldTrialParam();
  };

  // MediaSinkServiceBase implementation.
  void RecordDeviceCounts() override;
  void OnUserGesture() override;

  // MediaSinkServiceBase::Observer implementation.
  void OnSinkAddedOrUpdated(const MediaSinkInternal& sink) override;
  void OnSinkRemoved(const MediaSinkInternal& sink) override;

  // Attempts to resolve the given DIAL sink as a Cast sink. If successful,
  // the resulting Cast sink is added to the service.
  void TryConnectDialDiscoveredSink(const MediaSinkInternal& sink);

  // Marked virtual for testing.
  virtual void OpenChannels(const std::vector<MediaSinkInternal>& cast_sinks,
                            SinkSource sink_source);

  // CastSocket::Observer implementation.
  void OnError(const cast_channel::CastSocket& socket,
               cast_channel::ChannelError error_state) override;
  void OnMessage(const cast_channel::CastSocket& socket,
                 const cast_channel::CastMessage& message) override;

  // DiscoveryNetworkMonitor::Observer implementation
  void OnNetworksChanged(const std::string& network_id) override;

  // Returns cast socket open parameters. Parameters are read from Finch.
  // Connect / liveness timeout value are dynamically calculated
  // based on results of previous connection attempts.
  // |sink|: Sink to open cast channel to.
  cast_channel::CastSocketOpenParams CreateCastSocketOpenParams(
      const MediaSinkInternal& sink);

  // Opens cast channel. This method will not open a channel if there is already
  // a pending request for |ip_endpoint|, or if a channel for |ip_endpoint|
  // already exists.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |backoff_entry|: backoff entry passed to |OnChannelOpened| callback.
  void OpenChannel(const MediaSinkInternal& cast_sink,
                   std::unique_ptr<net::BackoffEntry> backoff_entry,
                   SinkSource sink_source);

  // Invoked when opening cast channel on IO thread completes.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |backoff_entry|: backoff entry passed to |OnChannelErrorMayRetry| callback
  // if open channel fails.
  // |start_time|: time at which corresponding |OpenChannel| was called.
  // |socket|: raw pointer of newly created cast channel. Does not take
  // ownership of |socket|.
  void OnChannelOpened(const MediaSinkInternal& cast_sink,
                       std::unique_ptr<net::BackoffEntry> backoff_entry,
                       SinkSource sink_source,
                       base::Time start_time,
                       cast_channel::CastSocket* socket);

  // Invoked by |OnChannelOpened| if opening cast channel failed. It will retry
  // opening channel in a delay specified by |backoff_entry| if current failure
  // count is less than max retry attempts. Or invoke |OnChannelError| if retry
  // is not allowed.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |backoff_entry|: backoff entry holds failure count and calculates back-off
  // for next retry.
  // |error_state|: erorr encountered when opending cast channel.
  void OnChannelErrorMayRetry(MediaSinkInternal cast_sink,
                              std::unique_ptr<net::BackoffEntry> backoff_entry,
                              cast_channel::ChannelError error_state,
                              SinkSource sink_source);

  // Invoked when opening cast channel succeeds.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |socket|: raw pointer of newly created cast channel. Does not take
  // ownership of |socket|.
  void OnChannelOpenSucceeded(MediaSinkInternal cast_sink,
                              cast_channel::CastSocket* socket,
                              SinkSource sink_source);

  // Invoked when opening cast channel fails after all retry
  // attempts.
  // |ip_endpoint|: ip endpoint of cast channel failing to connect to.
  // |sink|: The sink for which channel open failed.
  void OnChannelOpenFailed(const net::IPEndPoint& ip_endpoint,
                           const MediaSinkInternal& sink);

  // Returns whether the given DIAL-discovered |sink| is probably a non-Cast
  // device. This is heuristically determined by two things: |sink| has been
  // discovered via DIAL exclusively, and we failed to open a cast channel to
  // |sink| a number of times past a pre-determined threshold.
  // TODO(crbug.com/774233): This is a temporary and not a definitive way to
  // tell if a device is a Cast/non-Cast device. We need to collect some metrics
  // for the device description URL advertised by Cast devices to determine the
  // long term solution for restricting dual discovery.
  bool IsProbablyNonCastDevice(const MediaSinkInternal& sink) const;

  base::WeakPtr<CastMediaSinkServiceImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Set of IP endpoints pending to be connected to.
  std::set<net::IPEndPoint> pending_for_open_ip_endpoints_;

  // Set of IP endpoints found in current round of mDNS service. Used by
  // RecordDeviceCounts().
  std::set<net::IPEndPoint> known_ip_endpoints_;

  // Raw pointer of leaky singleton CastSocketService, which manages adding and
  // removing Cast channels.
  cast_channel::CastSocketService* const cast_socket_service_;

  // Raw pointer to DiscoveryNetworkMonitor, which is a global leaky singleton
  // and manages network change notifications.
  DiscoveryNetworkMonitor* const network_monitor_;

  std::string current_network_id_ = DiscoveryNetworkMonitor::kNetworkIdUnknown;

  // Cache of known sinks by network ID.
  std::map<std::string, std::vector<MediaSinkInternal>> sink_cache_;

  CastDeviceCountMetrics metrics_;

  RetryParams retry_params_;

  OpenParams open_params_;

  net::BackoffEntry::Policy backoff_policy_;

  // If |true|, |this| will try to open channel to sinks on all IPs, and not
  // just private IPs.
  bool allow_all_ips_ = false;

  // Map of consecutive cast channel failure count keyed by sink ID. Used to
  // dynamically adjust timeout values. If a Cast channel opens successfully,
  // the failure count is reset by removing the entry from the map.
  base::flat_map<MediaSink::Id, int> failure_count_map_;

  // Used by |IsProbablyNonCastDevice()| to keep track of how many times we
  // failed to open a cast channel for a sink that is discovered via DIAL
  // exclusively. The count is reset for a sink when it is discovered via mDNS,
  // or if we detected a network change.
  base::flat_map<MediaSink::Id, int> dial_sink_failure_count_;

  // Non-owned pointer to DIAL MediaSinkService. Observed by |this| for dual
  // discovery.
  MediaSinkServiceBase* const dial_media_sink_service_;

  // The SequencedTaskRunner on which methods are run. This shares the
  // same SequencedTaskRunner as the one used by |cast_socket_service_|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::Clock* clock_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastMediaSinkServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkServiceImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_
