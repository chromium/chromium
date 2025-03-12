// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/download_protection_metrics_data.h"

#include "base/metrics/histogram_functions.h"
#include "components/download/public/common/download_item.h"

namespace safe_browsing {

DownloadProtectionMetricsData::DownloadProtectionMetricsData() = default;

DownloadProtectionMetricsData::~DownloadProtectionMetricsData() {
  LogToHistogram();
}

const void* const kAndroidDownloadProtectionMetricsDataKey =
    &kAndroidDownloadProtectionMetricsDataKey;

// static
DownloadProtectionMetricsData* DownloadProtectionMetricsData::GetOrCreate(
    download::DownloadItem* item) {
  CHECK(item);
  DownloadProtectionMetricsData* data =
      static_cast<DownloadProtectionMetricsData*>(
          item->GetUserData(kAndroidDownloadProtectionMetricsDataKey));
  if (!data) {
    data = new DownloadProtectionMetricsData();
    item->SetUserData(kAndroidDownloadProtectionMetricsDataKey,
                      base::WrapUnique(data));
  }
  CHECK(data);
  return data;
}

// static
void DownloadProtectionMetricsData::SetOutcome(
    download::DownloadItem* item,
    AndroidDownloadProtectionOutcome outcome) {
  CHECK(item);
  DownloadProtectionMetricsData* data = GetOrCreate(item);
  data->outcome_ = outcome;
}

void DownloadProtectionMetricsData::LogToHistogram() {
  if (did_log_outcome_) {
    return;
  }
  base::UmaHistogramEnumeration(
      "SBClientDownload.Android.DownloadProtectionOutcome", outcome_);
  did_log_outcome_ = true;
}

}  // namespace safe_browsing
