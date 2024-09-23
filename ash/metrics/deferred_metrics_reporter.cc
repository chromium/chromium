// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/deferred_metrics_reporter.h"

#include <string>
#include <utility>

#include "base/check.h"

namespace ash {

DeferredMetricsReporter::DeferredMetricsReporter() = default;

DeferredMetricsReporter::~DeferredMetricsReporter() = default;

void DeferredMetricsReporter::SetPrefix(std::string new_prefix) {
  CHECK(!new_prefix.empty());
  prefix_ = std::move(new_prefix);
}

void DeferredMetricsReporter::ReportOrSchedule(std::unique_ptr<Metric> m) {
  if (ready_) {
    m->Report(prefix_);
  } else {
    scheduled_metrics_.push_back(std::move(m));
  }
}

void DeferredMetricsReporter::MarkReadyToReport() {
  CHECK(!prefix_.empty());
  ready_ = true;
  for (auto& m : scheduled_metrics_) {
    m->Report(prefix_);
  }
  scheduled_metrics_.clear();
}

}  // namespace ash
