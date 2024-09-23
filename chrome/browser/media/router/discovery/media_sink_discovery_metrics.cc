// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"

namespace {
// How long to wait between device counts metrics are recorded. Set to 1 hour.
const int kDeviceCountMetricThresholdMins = 60;
}  // namespace

namespace media_router {

DeviceCountMetrics::DeviceCountMetrics()
    : clock_(base::DefaultClock::GetInstance()) {}
DeviceCountMetrics::~DeviceCountMetrics() = default;

void DeviceCountMetrics::RecordDeviceCountsIfNeeded(
    size_t available_device_count,
    size_t known_device_count) {
  base::Time now = clock_->Now();
  if (now - device_count_metrics_record_time_ <
      base::Minutes(kDeviceCountMetricThresholdMins)) {
    return;
  }
  RecordDeviceCounts(available_device_count, known_device_count);
  device_count_metrics_record_time_ = now;
}

void DeviceCountMetrics::SetClockForTest(base::Clock* clock) {
  clock_ = clock;
}

// static
const char DialDeviceCountMetrics::kHistogramDialAvailableDeviceCount[] =
    "MediaRouter.Dial.AvailableDevicesCount";
const char DialDeviceCountMetrics::kHistogramDialKnownDeviceCount[] =
    "MediaRouter.Dial.KnownDevicesCount";

void DialDeviceCountMetrics::RecordDeviceCounts(size_t available_device_count,
                                                size_t known_device_count) {
  UMA_HISTOGRAM_COUNTS_100(kHistogramDialAvailableDeviceCount,
                           available_device_count);
  UMA_HISTOGRAM_COUNTS_100(kHistogramDialKnownDeviceCount, known_device_count);
}

// static
const char CastDeviceCountMetrics::kHistogramCastKnownDeviceCount[] =
    "MediaRouter.Cast.Discovery.KnownDevicesCount";
const char CastDeviceCountMetrics::kHistogramCastConnectedDeviceCount[] =
    "MediaRouter.Cast.Discovery.ConnectedDevicesCount";
const char CastDeviceCountMetrics::kHistogramCastDiscoverySinkSource[] =
    "MediaRouter.Cast.Discovery.SinkSource";

void CastDeviceCountMetrics::RecordDeviceCounts(size_t available_device_count,
                                                size_t known_device_count) {
  UMA_HISTOGRAM_COUNTS_100(kHistogramCastConnectedDeviceCount,
                           available_device_count);
  UMA_HISTOGRAM_COUNTS_100(kHistogramCastKnownDeviceCount, known_device_count);
}

void CastDeviceCountMetrics::RecordCastSinkDiscoverySource(
    SinkSource sink_source) {
  DCHECK_LT(sink_source, kTotalCount);
  UMA_HISTOGRAM_ENUMERATION(kHistogramCastDiscoverySinkSource, sink_source,
                            kTotalCount);
}

// static
const char CastAnalytics::kHistogramCastChannelConnectResult[] =
    "MediaRouter.Cast.Channel.ConnectResult";
const char CastAnalytics::kHistogramCastChannelError[] =
    "MediaRouter.Cast.Channel.Error";
const char CastAnalytics::kHistogramCastDeviceNameLength[] =
    "MediaRouter.Cast.DeviceNameLength";
const char CastAnalytics::kHistogramCastMdnsChannelOpenSuccess[] =
    "MediaRouter.Cast.Mdns.Channel.Open_Success";
const char CastAnalytics::kHistogramCastMdnsChannelOpenFailure[] =
    "MediaRouter.Cast.Mdns.Channel.Open_Failure";

// static
void CastAnalytics::RecordCastChannelConnectResult(
    MediaRouterChannelConnectResults result) {
  DCHECK_LT(result, MediaRouterChannelConnectResults::TOTAL_COUNT);
  UMA_HISTOGRAM_ENUMERATION(kHistogramCastChannelConnectResult, result,
                            MediaRouterChannelConnectResults::TOTAL_COUNT);
}

// static
void CastAnalytics::RecordDeviceChannelError(
    MediaRouterChannelError channel_error) {
  DCHECK_LT(channel_error, MediaRouterChannelError::TOTAL_COUNT);
  UMA_HISTOGRAM_ENUMERATION(kHistogramCastChannelError, channel_error,
                            MediaRouterChannelError::TOTAL_COUNT);
}

// static
void CastAnalytics::RecordDeviceChannelOpenDuration(
    bool success,
    const base::TimeDelta& duration) {
  if (success) {
    UMA_HISTOGRAM_TIMES(kHistogramCastMdnsChannelOpenSuccess, duration);
  } else {
    UMA_HISTOGRAM_TIMES(kHistogramCastMdnsChannelOpenFailure, duration);
  }
}

// static
void CastAnalytics::RecordDeviceNameLength(size_t length) {
  base::UmaHistogramCounts100(kHistogramCastDeviceNameLength, length);
}

void WiredDisplayDeviceCountMetrics::RecordDeviceCounts(
    size_t available_device_count,
    size_t known_device_count) {
  // Record just the available device count, because for wired displays, all
  // the known displays are available.
  UMA_HISTOGRAM_COUNTS_100(kHistogramWiredDisplayDeviceCount,
                           available_device_count);
}

const char WiredDisplayDeviceCountMetrics::kHistogramWiredDisplayDeviceCount[] =
    "MediaRouter.WiredDisplay.AvailableDevicesCount";

}  // namespace media_router
