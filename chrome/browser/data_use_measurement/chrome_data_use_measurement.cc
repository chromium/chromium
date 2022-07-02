// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/traffic_stats.h"
#endif

using content::BrowserThread;

// static
ChromeDataUseMeasurement& ChromeDataUseMeasurement::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  static base::NoDestructor<ChromeDataUseMeasurement>
      s_chrome_data_use_measurement;
  return *s_chrome_data_use_measurement;
}

ChromeDataUseMeasurement::ChromeDataUseMeasurement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("browser", "ChromeDataUseMeasurement");

#if BUILDFLAG(IS_ANDROID)
  int64_t bytes = 0;
  // Query Android traffic stats.
  if (net::android::traffic_stats::GetCurrentUidRxBytes(&bytes))
    rx_bytes_os_ = bytes;

  if (net::android::traffic_stats::GetCurrentUidTxBytes(&bytes))
    tx_bytes_os_ = bytes;
#endif
}

ChromeDataUseMeasurement::~ChromeDataUseMeasurement() = default;

void ChromeDataUseMeasurement::ReportNetworkServiceDataUse(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Negative byte numbers is not a critical problem (i.e., should have no
  // security implications) but is not expected. TODO(rajendrant): remove these
  // DCHECKs or consider using uint in Mojo instead.
  DCHECK_GE(recv_bytes, 0);
  DCHECK_GE(sent_bytes, 0);

  ReportDataUsage(TrafficDirection::kUpstream, sent_bytes);
  ReportDataUsage(TrafficDirection::kDownstream, recv_bytes);
}

void ChromeDataUseMeasurement::ReportDataUsage(TrafficDirection dir,
                                               int64_t message_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("browser", "ChromeDataUseMeasurement::ReportDataUsage");

  if (message_size_bytes <= 0)
    return;

  if (dir == TrafficDirection::kDownstream) {
    base::UmaHistogramCustomCounts("DataUse.BytesReceived3.Delegate",
                                   message_size_bytes, 50, 10 * 1000 * 1000,
                                   50);
  }

  if (dir == TrafficDirection::kUpstream)
    UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent3.Delegate", message_size_bytes);

#if BUILDFLAG(IS_ANDROID)
  bytes_transferred_since_last_traffic_stats_query_ += message_size_bytes;
  MaybeRecordNetworkBytesOS();
#endif
}

#if BUILDFLAG(IS_ANDROID)
void ChromeDataUseMeasurement::MaybeRecordNetworkBytesOS() {
  // Minimum number of bytes that should be reported by the network delegate
  // before Android's TrafficStats API is queried (if Chrome is not in
  // background). This reduces the overhead of repeatedly calling the API.
  static const int64_t kMinDelegateBytes = 25000;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bytes_transferred_since_last_traffic_stats_query_ < kMinDelegateBytes) {
    return;
  }
  bytes_transferred_since_last_traffic_stats_query_ = 0;
  int64_t bytes = 0;
  // Query Android traffic stats directly instead of registering with the
  // DataUseAggregator since the latter does not provide notifications for
  // the incognito traffic.
  if (net::android::traffic_stats::GetCurrentUidRxBytes(&bytes)) {
    if (rx_bytes_os_ != 0) {
      DCHECK_GE(bytes, rx_bytes_os_);
      if (bytes > rx_bytes_os_) {
        int64_t incremental_bytes = bytes - rx_bytes_os_;
        // Do not record samples with value 0.
        base::UmaHistogramCustomCounts("DataUse.BytesReceived3.OS",
                                       incremental_bytes, 50, 10 * 1000 * 1000,
                                       50);
      }
    }
    rx_bytes_os_ = bytes;
  }

  if (net::android::traffic_stats::GetCurrentUidTxBytes(&bytes)) {
    if (tx_bytes_os_ != 0) {
      DCHECK_GE(bytes, tx_bytes_os_);
      if (bytes > tx_bytes_os_) {
        int64_t incremental_bytes = bytes - tx_bytes_os_;
        // Do not record samples with value 0.
        UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent3.OS", incremental_bytes);
      }
    }
    tx_bytes_os_ = bytes;
  }
}

#endif
