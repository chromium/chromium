// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {

void RecordLoadTemplateHistogram() {
  base::UmaHistogramBoolean(kLoadTemplateGridHistogramName, true);
}

void RecordDeleteTemplateHistogram() {
  base::UmaHistogramBoolean(kDeleteTemplateHistogramName, true);
}

void RecordLaunchTemplateHistogram() {
  base::UmaHistogramBoolean(kLaunchTemplateHistogramName, true);
}

void RecordNewTemplateHistogram() {
  base::UmaHistogramBoolean(kNewTemplateHistogramName, true);
}

void RecordAddOrUpdateTemplateStatusHistogram(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  base::UmaHistogramEnumeration(kAddOrUpdateTemplateStatusHistogramName,
                                status);
}

void RecordUserTemplateCountHistogram(size_t entry_count,
                                      size_t max_entry_count) {
  UMA_HISTOGRAM_EXACT_LINEAR(kUserTemplateCountHistogramName, entry_count,
                             max_entry_count);
}

void RecordWindowAndTabCountHistogram(DeskTemplate* desk_template) {
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
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
      absl::optional<std::vector<GURL>> urls = window_iter.second->urls;
      if (!urls || urls->empty())
        continue;

      ++window_count;
      tab_count += urls->size();
      total_count += urls->size();
    }
  }

  base::UmaHistogramCounts100(kWindowCountHistogramName, window_count);
  base::UmaHistogramCounts100(kTabCountHistogramName, tab_count);
  base::UmaHistogramCounts100(kWindowAndTabCountHistogramName, total_count);
}

void RecordUnsupportedAppDialogShowHistogram() {
  base::UmaHistogramBoolean(kUnsupportedAppDialogShowHistogramName, true);
}

void RecordReplaceTemplateHistogram() {
  base::UmaHistogramBoolean(kReplaceTemplateHistogramName, true);
}

}  // namespace ash
