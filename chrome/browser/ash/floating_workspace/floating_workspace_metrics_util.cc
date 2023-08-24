// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_metrics_util.h"
#include "base/metrics/histogram_functions.h"

namespace ash::floating_workspace_metrics_util {

void RecordFloatingWorkspaceV1InitializedHistogram() {
  base::UmaHistogramBoolean(kFloatingWorkspaceV1Initialized, true);
}

void RecordFloatingWorkspaceV1RestoredSessionType(
    RestoredBrowserSessionType type) {
  base::UmaHistogramEnumeration(kFloatingWorkspaceV1RestoredSessionType, type);
}

void RecordFloatingWorkspaceV2TemplateLaunchFailureType(
    LaunchTemplateFailureType type) {
  base::UmaHistogramEnumeration(kFloatingWorkspaceV2TemplateLaunchFailureStatus,
                                type);
}

// TODO(b/274501763): rename for better clarity since this does not just record
// for timeout reasons.
void RecordFloatingWorkspaceV2TemplateLaunchTimeout(
    LaunchTemplateTimeoutType type) {
  base::UmaHistogramEnumeration(kFloatingWorkspaceV2TemplateLaunchTimedOut,
                                type);
}

void RecordFloatingWorkspaceV2TemplateLoadTime(base::TimeDelta duration) {
  constexpr size_t bucket_count = 50;
  constexpr base::TimeDelta min_bucket = base::Seconds(0);
  constexpr base::TimeDelta max_bucket = base::Seconds(300);  // Five minute.

  static auto* histogram = base::Histogram::FactoryGet(
      kFloatingWorkspaceV2TemplateLoadTime, min_bucket.InSeconds(),
      max_bucket.InSeconds(), bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->Add(duration.InSeconds());
}

void RecordFloatingWorkspaceV2TemplateUploadStatusHistogram(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  base::UmaHistogramEnumeration(kFloatingWorkspaceV2TemplateUploadStatus,
                                status);
}

void RecordFloatingWorkspaceV2InitializedHistogram() {
  base::UmaHistogramBoolean(kFloatingWorkspaceV2Initialized, true);
}
void RecordFloatingWorkspaceV2TemplateNotFound() {
  base::UmaHistogramBoolean(kFloatingWorkspaceV2TemplateNotFound, true);
}

}  // namespace ash::floating_workspace_metrics_util
