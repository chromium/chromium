// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_RUST_SHIM_H_
#define BASE_METRICS_HISTOGRAM_RUST_SHIM_H_

#include <stddef.h>
#include <stdint.h>

#include "third_party/rust/cxx/v1/cxx.h"

namespace base::rust {

void record_bool(::rust::Str name, bool sample);
void record_exact_linear(::rust::Str name,
                         int32_t sample,
                         int32_t exclusive_max);
void record_percentage(::rust::Str name, int32_t percent);
void record_sparse(::rust::Str name, int32_t sample);
void record_custom_counts(::rust::Str name,
                          int32_t sample,
                          int32_t min,
                          int32_t exclusive_max,
                          size_t buckets);
void record_counts_100(::rust::Str name, int32_t sample);
void record_counts_1000(::rust::Str name, int32_t sample);
void record_counts_10000(::rust::Str name, int32_t sample);
void record_counts_100000(::rust::Str name, int32_t sample);
void record_counts_1m(::rust::Str name, int32_t sample);
void record_counts_10m(::rust::Str name, int32_t sample);
void record_memory_kb(::rust::Str name, int32_t sample_kb);
void record_memory_mb(::rust::Str name, int32_t sample_mb);
void record_memory_large_mb(::rust::Str name, int32_t sample_mb);

}  // namespace base::rust

#endif  // BASE_METRICS_HISTOGRAM_RUST_SHIM_H_
