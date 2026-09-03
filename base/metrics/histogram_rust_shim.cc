// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_rust_shim.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/metrics/histogram_functions.h"

namespace base::rust {

void record_bool(::rust::Str name, bool sample) {
  UmaHistogramBoolean(std::string_view(name), sample);
}

void record_exact_linear(::rust::Str name,
                         int32_t sample,
                         int32_t exclusive_max) {
  UmaHistogramExactLinear(std::string_view(name), sample, exclusive_max);
}

void record_percentage(::rust::Str name, int32_t percent) {
  UmaHistogramPercentage(std::string_view(name), percent);
}

void record_sparse(::rust::Str name, int32_t sample) {
  UmaHistogramSparse(std::string_view(name), sample);
}

void record_custom_counts(::rust::Str name,
                          int32_t sample,
                          int32_t min,
                          int32_t exclusive_max,
                          size_t buckets) {
  UmaHistogramCustomCounts(std::string_view(name), sample, min, exclusive_max,
                           buckets);
}

void record_counts_100(::rust::Str name, int32_t sample) {
  UmaHistogramCounts100(std::string_view(name), sample);
}

void record_counts_1000(::rust::Str name, int32_t sample) {
  UmaHistogramCounts1000(std::string_view(name), sample);
}

void record_counts_10000(::rust::Str name, int32_t sample) {
  UmaHistogramCounts10000(std::string_view(name), sample);
}

void record_counts_100000(::rust::Str name, int32_t sample) {
  UmaHistogramCounts100000(std::string_view(name), sample);
}

void record_counts_1m(::rust::Str name, int32_t sample) {
  UmaHistogramCounts1M(std::string_view(name), sample);
}

void record_counts_10m(::rust::Str name, int32_t sample) {
  UmaHistogramCounts10M(std::string_view(name), sample);
}

void record_memory_kb(::rust::Str name, int32_t sample_kb) {
  UmaHistogramMemoryKB(std::string_view(name), sample_kb);
}

void record_memory_mb(::rust::Str name, int32_t sample_mb) {
  UmaHistogramMemoryMB(std::string_view(name), sample_mb);
}

void record_memory_large_mb(::rust::Str name, int32_t sample_mb) {
  UmaHistogramMemoryLargeMB(std::string_view(name), sample_mb);
}

}  // namespace base::rust
