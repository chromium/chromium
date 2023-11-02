// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {

void RecordLoadSavedDeskLibraryHistogram() {
  base::UmaHistogramBoolean(kLoadTemplateGridHistogramName, true);
}

void RecordDeleteSavedDeskHistogram(DeskTemplateType type) {
  base::UmaHistogramBoolean(type == DeskTemplateType::kTemplate
                                ? kDeleteTemplateHistogramName
                                : kDeleteSaveAndRecallHistogramName,
                            true);
}

void RecordLaunchSavedDeskHistogram(DeskTemplateType type) {
  base::UmaHistogramBoolean(type == DeskTemplateType::kTemplate
                                ? kLaunchTemplateHistogramName
                                : kLaunchSaveAndRecallHistogramName,
                            true);
}

void RecordNewSavedDeskHistogram(DeskTemplateType type) {
  base::UmaHistogramBoolean(type == DeskTemplateType::kTemplate
                                ? kNewTemplateHistogramName
                                : kNewSaveAndRecallHistogramName,
                            true);
}

void RecordReplaceSavedDeskHistogram(DeskTemplateType type) {
  base::UmaHistogramBoolean(type == DeskTemplateType::kTemplate
                                ? kReplaceTemplateHistogramName
                                : kReplaceSaveAndRecallHistogramName,
                            true);
}

void RecordAddOrUpdateTemplateStatusHistogram(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  base::UmaHistogramEnumeration(kAddOrUpdateTemplateStatusHistogramName,
                                status);
}

void RecordUserSavedDeskCountHistogram(DeskTemplateType type,
                                       size_t entry_count,
                                       size_t max_entry_count) {
  if (type == DeskTemplateType::kTemplate) {
    UMA_HISTOGRAM_EXACT_LINEAR(kUserTemplateCountHistogramName, entry_count,
                               max_entry_count);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(kUserSaveAndRecallCountHistogramName,
                               entry_count, max_entry_count);
  }
}

void RecordWindowAndTabCountHistogram(const DeskTemplate& desk_template) {
  const app_restore::RestoreData* restore_data =
      desk_template.desk_restore_data();
  DCHECK(restore_data);

  int window_count = 0;
  int tab_count = 0;
  int total_count = 0;

  const auto& launch_list = restore_data->app_id_to_launch_list();
  for (const auto& iter : launch_list) {
    // Since apps aren't guaranteed to have the url field set up correctly, this
    // is necessary to ensure things are not double-counted.
    if (iter.first != app_constants::kChromeAppId) {
      ++window_count;
      ++total_count;
      continue;
    }

    for (const auto& window_iter : iter.second) {
      const absl::optional<std::vector<GURL>>& urls = window_iter.second->urls;
      if (!urls || urls->empty())
        continue;

      ++window_count;
      tab_count += urls->size();
      total_count += urls->size();
    }
  }

  if (desk_template.type() == DeskTemplateType::kTemplate) {
    base::UmaHistogramCounts100(kTemplateWindowCountHistogramName,
                                window_count);
    base::UmaHistogramCounts100(kTemplateTabCountHistogramName, tab_count);
    base::UmaHistogramCounts100(kTemplateWindowAndTabCountHistogramName,
                                total_count);
  } else {
    base::UmaHistogramCounts100(kSaveAndRecallWindowCountHistogramName,
                                window_count);
    base::UmaHistogramCounts100(kSaveAndRecallTabCountHistogramName, tab_count);
    base::UmaHistogramCounts100(kSaveAndRecallWindowAndTabCountHistogramName,
                                total_count);
  }
}

void RecordUnsupportedAppDialogShowHistogram(DeskTemplateType type) {
  base::UmaHistogramBoolean(
      type == DeskTemplateType::kTemplate
          ? kTemplateUnsupportedAppDialogShowHistogramName
          : kSaveAndRecallUnsupportedAppDialogShowHistogramName,
      true);
}

void RecordTimeBetweenSaveAndRecall(base::TimeDelta duration) {
  // Constants for the histogram. Do not change these values without also
  // changing the histogram name.
  constexpr size_t bucket_count = 50;
  constexpr base::TimeDelta min_bucket = base::Seconds(1);
  constexpr base::TimeDelta max_bucket = base::Hours(24 * 7);  // One week.

  // Lazily construct the histogram.
  static auto* histogram = base::Histogram::FactoryGet(
      kTimeBetweenSaveAndRecallHistogramName, min_bucket.InSeconds(),
      max_bucket.InSeconds(), bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->Add(duration.InSeconds());
}

}  // namespace ash
