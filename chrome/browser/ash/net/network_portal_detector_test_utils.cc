// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_portal_detector_test_utils.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/ranges/algorithm.h"

namespace ash {

EnumHistogramChecker::EnumHistogramChecker(const std::string& histogram,
                                           int count,
                                           base::HistogramSamples* base)
    : histogram_(histogram), expect_(count), base_(base) {}

EnumHistogramChecker::~EnumHistogramChecker() {}

EnumHistogramChecker* EnumHistogramChecker::Expect(int key, int value) {
  expect_[key] = value;
  return this;
}

bool EnumHistogramChecker::Check() {
  bool empty = false;
  size_t num_zeroes = static_cast<size_t>(base::ranges::count(expect_, 0));
  if (num_zeroes == expect_.size())
    empty = true;
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(histogram_);
  if (!histogram) {
    if (!empty) {
      LOG(ERROR) << "Non-empty expectations for " << histogram_ << " "
                 << "which does not exists.";
      return false;
    }
    return true;
  }
  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotSamples();
  if (!samples.get()) {
    if (!empty) {
      LOG(ERROR) << "Non-empty expectations for " << histogram_ << " "
                 << "for which samples do not exist.";
      return false;
    }
    return true;
  }

  bool ok = true;
  for (size_t i = 0; i < expect_.size(); ++i) {
    const int base = base_ ? base_->GetCount(i) : 0;
    const int actual = samples->GetCount(i) - base;
    if (actual != expect_[i]) {
      LOG(ERROR) << "Histogram: " << histogram_ << ", value #" << i << ", "
                 << "expected: " << expect_[i] << ", actual: " << actual;
      ok = false;
    }
  }
  return ok;
}

}  // namespace ash
