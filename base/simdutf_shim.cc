// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/simdutf_shim.h"

#include "third_party/simdutf/simdutf.h"

namespace base {

size_t simdutf_base64_length_from_binary(size_t length) {
  return simdutf::base64_length_from_binary(length);
}

size_t simdutf_binary_to_base64(span<const uint8_t> input,
                                span<char> binary_output) {
  return simdutf::binary_to_base64(input, binary_output);
}

}  // namespace base
