// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_METRICS_HASHES_H_
#define BASE_METRICS_METRICS_HASHES_H_

#include <stdint.h>

#include "base/base_export.h"
#include "base/strings/string_piece.h"

namespace base {

// Computes a uint64_t hash of a given string based on its MD5 hash. Suitable
// for metric names.
BASE_EXPORT uint64_t HashMetricName(base::StringPiece name);

// Computes a uint32_t hash of a given string based on its MD5 hash. This
// can be more suitable for contexts where memory use is a concern.
BASE_EXPORT uint32_t HashMetricNameAs32Bits(base::StringPiece name);

}  // namespace base

#endif  // BASE_METRICS_METRICS_HASHES_H_
