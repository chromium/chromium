// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_HISTOGRAM_RUST_SHIM_H_
#define BASE_METRICS_HISTOGRAM_RUST_SHIM_H_

#include <stdint.h>

#include "third_party/rust/cxx/v1/cxx.h"

namespace base::rust {

void record_bool(::rust::Str name, bool sample);
void record_exact_linear(::rust::Str name,
                         int32_t sample,
                         int32_t exclusive_max);
void record_percentage(::rust::Str name, int32_t percent);
void record_sparse(::rust::Str name, int32_t sample);

}  // namespace base::rust

#endif  // BASE_METRICS_HISTOGRAM_RUST_SHIM_H_
