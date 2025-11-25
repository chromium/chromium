// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/puma_histogram_functions.h"

#include "base/metrics/histogram.h"

namespace base {

void PumaHistogramBoolean(PumaType puma_type,
                          std::string_view name,
                          bool sample) {
  HistogramBase* histogram =
      BooleanHistogram::FactoryGet(name, PumaTypeToHistogramFlags(puma_type));
  histogram->Add(sample);
}

void PumaHistogramExactLinear(PumaType puma_type,
                              std::string_view name,
                              int sample,
                              int exclusive_max) {
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      name, 1, exclusive_max, static_cast<size_t>(exclusive_max + 1),
      PumaTypeToHistogramFlags(puma_type));
  histogram->Add(sample);
}

}  // namespace base
