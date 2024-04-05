// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/histogram_waiter.h"

namespace ash {

HistogramWaiter::HistogramWaiter(const char* metric_name) {
  histogram_observer_ =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          metric_name,
          base::BindRepeating(&HistogramWaiter::OnHistogramCallback,
                              base::Unretained(this)));
}

HistogramWaiter::~HistogramWaiter() {
  histogram_observer_.reset();
}

void HistogramWaiter::Wait() {
  run_loop_.Run();
}

void HistogramWaiter::OnHistogramCallback(const char* metric_name,
                                          uint64_t name_hash,
                                          base::HistogramBase::Sample sample) {
  run_loop_.Quit();
  histogram_observer_.reset();
}

}  // namespace ash
