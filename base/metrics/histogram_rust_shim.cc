// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_rust_shim.h"

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

}  // namespace base::rust
