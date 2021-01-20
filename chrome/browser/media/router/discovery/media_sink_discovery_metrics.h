// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_DISCOVERY_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_DISCOVERY_METRICS_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/time/clock.h"
#include "base/time/time.h"

namespace media_router {

// Possible values for Cast channel connect results.
enum class MediaRouterChannelConnectResults {
  FAILURE = 0,
  SUCCESS = 1,

  // Note = Add entries only immediately above this line.
  TOTAL_COUNT = 2
};

// Possible values for channel open errors.
enum class MediaRouterChannelError {
  UNKNOWN = 0,
  AUTHENTICATION = 1,
  CONNECT = 2,
  GENERAL_CERTIFICATE = 3,
  CERTIFICATE_TIMING = 4,
  NETWORK = 5,
  CONNECT_TIMEOUT = 6,
  PING_TIMEOUT = 7,

  // Note = Add entries only immediately above this line.
  TOTAL_COUNT = 8
};

class DeviceCountMetrics {
 public:
  DeviceCountMetrics();
  ~DeviceCountMetrics();

  // Records device counts if last record was more than one hour ago.
  void RecordDeviceCountsIfNeeded(size_t available_device_count,
                                  size_t known_device_count);

  // Allows tests to swap in a fake clock.
  void SetClockForTest(base::Clock* clock);

 protected:
  // Record device counts.
  virtual void RecordDeviceCounts(size_t available_device_count,
                                  size_t known_device_count) = 0;

 private:
  base::Time device_count_metrics_record_time_;

  base::Clock* clock_;
};

// Metrics for DIAL device counts.
class DialDeviceCountMetrics : public DeviceCountMetrics {
 public:
  static const char kHistogramDialAvailableDeviceCount[];
  static const char kHistogramDialKnownDeviceCount[];

  void RecordDeviceCounts(size_t available_device_count,
                          size_t known_device_count) override;
};

// Metrics for Cast device counts.
class CastDeviceCountMetrics : public DeviceCountMetrics {
 public:
  // Indicates the discovery source that led to the creation of a cast sink.
  // This is tied to the UMA histogram MediaRouter.Cast.Discovery.SinkSource, so
  // new entries should only be added to the end, but before kTotalCount.
  enum SinkSource {
    kNetworkCache = 0,
    kMdns = 1,
    kDial = 2,
    kConnectionRetry = 3,
    kMdnsDial = 4,  // Device was first discovered via mDNS, then by DIAL.
    kDialMdns = 5,  // Device was first discovered via DIAL, then by mDNS.

    kTotalCount = 6,
  };

  static const char kHistogramCastKnownDeviceCount[];
  static const char kHistogramCastConnectedDeviceCount[];
  static const char kHistogramCastCachedSinksAvailableCount[];
  static const char kHistogramCastDiscoverySinkSource[];

  void RecordDeviceCounts(size_t available_device_count,
                          size_t known_device_count) override;
  void RecordCachedSinksAvailableCount(size_t cached_sink_count);
  void RecordCastSinkDiscoverySource(SinkSource sink_source);
};

class CastAnalytics {
 public:
  static const char kHistogramCastChannelConnectResult[];
  static const char kHistogramCastChannelError[];
  static const char kHistogramCastMdnsChannelOpenSuccess[];
  static const char kHistogramCastMdnsChannelOpenFailure[];

  static void RecordCastChannelConnectResult(
      MediaRouterChannelConnectResults result);
  static void RecordDeviceChannelError(MediaRouterChannelError channel_error);
  static void RecordDeviceChannelOpenDuration(bool success,
                                              const base::TimeDelta& duration);
};

// Metrics for wired display (local screen) sink counts.
class WiredDisplayDeviceCountMetrics : public DeviceCountMetrics {
 protected:
  // |known_device_count| is not recorded, since it should be the same as
  // |available_device_count|.
  void RecordDeviceCounts(size_t available_device_count,
                          size_t known_device_count) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WiredDisplayDeviceCountMetricsTest,
                           RecordWiredDisplaySinkCount);

  static const char kHistogramWiredDisplayDeviceCount[];
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MEDIA_SINK_DISCOVERY_METRICS_H_
