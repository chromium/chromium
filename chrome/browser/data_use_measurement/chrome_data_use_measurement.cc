// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "components/data_use_measurement/core/url_request_classifier.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/features.h"

namespace data_use_measurement {

namespace {
void UpdateMetricsUsagePrefs(int64_t total_bytes,
                             bool is_cellular,
                             bool is_metrics_service_usage) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Some unit tests use IOThread but do not initialize MetricsService. In that
  // case it's fine to skip the update.
  auto* metrics_service = g_browser_process->metrics_service();
  if (metrics_service) {
    metrics_service->UpdateMetricsUsagePrefs(
        base::saturated_cast<int>(total_bytes), is_cellular,
        is_metrics_service_usage);
  }
}

// This function is for forwarding metrics usage pref changes to the metrics
// service on the appropriate thread.
// TODO(gayane): Reduce the frequency of posting tasks from IO to UI thread.
void UpdateMetricsUsagePrefsOnUIThread(int64_t total_bytes,
                                       bool is_cellular,
                                       bool is_metrics_service_usage) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::PostTaskWithTraits(
      FROM_HERE, content::BrowserThread::UI,
      base::BindOnce(UpdateMetricsUsagePrefs, total_bytes, is_cellular,
                     is_metrics_service_usage));
}
}  // namespace

// static
std::unique_ptr<ChromeDataUseMeasurement>
ChromeDataUseMeasurement::CreateForNetworkService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Do not create when NetworkService is disabled, since data use of URLLoader
  // is reported via the network delegate callbacks.
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return nullptr;

  return std::make_unique<ChromeDataUseMeasurement>(
      nullptr, nullptr, content::GetNetworkConnectionTracker());
}

ChromeDataUseMeasurement::ChromeDataUseMeasurement(
    std::unique_ptr<URLRequestClassifier> url_request_classifier,
    DataUseAscriber* ascriber,
    network::NetworkConnectionTracker* network_connection_tracker)
    : DataUseMeasurement(std::move(url_request_classifier),
                         ascriber,
                         network_connection_tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ChromeDataUseMeasurement::UpdateDataUseToMetricsService(
    int64_t total_bytes,
    bool is_cellular,
    bool is_metrics_service_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update data use of user traffic and services distinguishing cellular and
  // metrics services data use.
  UpdateMetricsUsagePrefsOnUIThread(total_bytes, is_cellular,
                                    is_metrics_service_usage);
}

void ChromeDataUseMeasurement::ReportNetworkServiceDataUse(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
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
      observer.OnServicesDataUse(recv_bytes, sent_bytes);
  }
  UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesReceived.Delegate", recv_bytes);
  UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent.Delegate", sent_bytes);
#if defined(OS_ANDROID)
  bytes_transferred_since_last_traffic_stats_query_ += recv_bytes + sent_bytes;
  MaybeRecordNetworkBytesOS();
#endif
}

}  // namespace data_use_measurement
