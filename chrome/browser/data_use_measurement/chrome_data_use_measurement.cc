// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/metrics/data_use_tracker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"

using content::BrowserThread;

namespace data_use_measurement {

namespace {
// Global instance to be used when network service is enabled, this will never
// be deleted. When network service is disabled, this should always be null.
ChromeDataUseMeasurement* g_chrome_data_use_measurement = nullptr;

DataUseUserData::DataUseContentType GetContentType(const std::string& mime_type,
                                                   bool is_main_frame_resource,
                                                   bool is_app_background,
                                                   bool is_tab_visible) {
  if (mime_type == "text/html")
    return is_main_frame_resource ? DataUseUserData::MAIN_FRAME_HTML
                                  : DataUseUserData::NON_MAIN_FRAME_HTML;
  if (mime_type == "text/css")
    return DataUseUserData::CSS;
  if (base::StartsWith(mime_type, "image/", base::CompareCase::SENSITIVE))
    return DataUseUserData::IMAGE;
  if (base::EndsWith(mime_type, "javascript", base::CompareCase::SENSITIVE) ||
      base::EndsWith(mime_type, "ecmascript", base::CompareCase::SENSITIVE)) {
    return DataUseUserData::JAVASCRIPT;
  }
  if (mime_type.find("font") != std::string::npos)
    return DataUseUserData::FONT;
  if (base::StartsWith(mime_type, "audio/", base::CompareCase::SENSITIVE))
    return is_app_background
               ? DataUseUserData::AUDIO_APPBACKGROUND
               : (!is_tab_visible ? DataUseUserData::AUDIO_TABBACKGROUND
                                  : DataUseUserData::AUDIO);
  if (base::StartsWith(mime_type, "video/", base::CompareCase::SENSITIVE))
    return is_app_background
               ? DataUseUserData::VIDEO_APPBACKGROUND
               : (!is_tab_visible ? DataUseUserData::VIDEO_TABBACKGROUND
                                  : DataUseUserData::VIDEO);
  return DataUseUserData::OTHER;
}

}  // namespace

// static
void ChromeDataUseMeasurement::CreateInstance(PrefService* local_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  DCHECK(!g_chrome_data_use_measurement);

  g_chrome_data_use_measurement = new ChromeDataUseMeasurement(
      content::GetNetworkConnectionTracker(), local_state);
}

// static
ChromeDataUseMeasurement* ChromeDataUseMeasurement::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  return g_chrome_data_use_measurement;
}

// static
void ChromeDataUseMeasurement::DeleteInstance() {
  if (g_chrome_data_use_measurement) {
    delete g_chrome_data_use_measurement;
    g_chrome_data_use_measurement = nullptr;
  }
}

ChromeDataUseMeasurement::ChromeDataUseMeasurement(
    network::NetworkConnectionTracker* network_connection_tracker,
    PrefService* local_state)
    : DataUseMeasurement(network_connection_tracker),
      local_state_(local_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ChromeDataUseMeasurement::ReportNetworkServiceDataUse(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Negative byte numbres is not a critical problem (i.e. should have no security implications) but
  // is not expected. TODO(rajendrant): remove these DCHECKs or consider using uint in Mojo instead.
  DCHECK_GE(recv_bytes, 0);
  DCHECK_GE(sent_bytes, 0);

  bool is_user_request =
      DataUseMeasurement::IsUserRequest(network_traffic_annotation_id_hash);
  bool is_metrics_service_request =
      IsMetricsServiceRequest(network_traffic_annotation_id_hash);
  UpdateMetricsUsagePrefs(recv_bytes, IsCurrentNetworkCellular(),
                          is_metrics_service_request);
  UpdateMetricsUsagePrefs(sent_bytes, IsCurrentNetworkCellular(),
                          is_metrics_service_request);
  if (!is_user_request) {
    ReportDataUsageServices(network_traffic_annotation_id_hash, UPSTREAM,
                            CurrentAppState(), sent_bytes);
    ReportDataUsageServices(network_traffic_annotation_id_hash, DOWNSTREAM,
                            CurrentAppState(), recv_bytes);
  }
  if (!is_user_request || DataUseMeasurement::IsUserDownloadsRequest(
                              network_traffic_annotation_id_hash)) {
    for (auto& observer : services_data_use_observer_list_)
      observer.OnServicesDataUse(network_traffic_annotation_id_hash, recv_bytes,
                                 sent_bytes);
  }
  UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesReceived.Delegate", recv_bytes);
  UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent.Delegate", sent_bytes);
#if defined(OS_ANDROID)
  bytes_transferred_since_last_traffic_stats_query_ += recv_bytes + sent_bytes;
  MaybeRecordNetworkBytesOS();
#endif
}

void ChromeDataUseMeasurement::ReportUserTrafficDataUse(bool is_tab_visible,
                                                        int64_t recv_bytes) {
  RecordTrafficSizeMetric(true, true, is_tab_visible, recv_bytes);
}

void ChromeDataUseMeasurement::RecordContentTypeMetric(
    const std::string& mime_type,
    bool is_main_frame_resource,
    bool is_tab_visible,
    int64_t recv_bytes) {
  DataUseUserData::DataUseContentType content_type = GetContentType(
      mime_type, is_main_frame_resource,
      CurrentAppState() == DataUseUserData::BACKGROUND, is_tab_visible);
  UMA_HISTOGRAM_SCALED_ENUMERATION("DataUse.ContentType.UserTrafficKB",
                                   content_type, recv_bytes, 1024);
}

void ChromeDataUseMeasurement::UpdateMetricsUsagePrefs(
    int64_t total_bytes,
    bool is_cellular,
    bool is_metrics_service_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(local_state_);
  metrics::DataUseTracker::UpdateMetricsUsagePrefs(
      base::saturated_cast<int>(total_bytes), is_cellular,
      is_metrics_service_usage, local_state_);
}

}  // namespace data_use_measurement
