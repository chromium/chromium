// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SIMDUTF_SHIM_H_
#define BASE_SIMDUTF_SHIM_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"

// Shim so the //base GN target can avoid directly using simdutf.

namespace base {

[[nodiscard]] size_t simdutf_base64_length_from_binary(size_t length);

[[nodiscard]] size_t simdutf_binary_to_base64(span<const uint8_t> input,
                                              span<char> binary_output);

}  // namespace base

#endif  // BASE_SIMDUTF_SHIM_H_
