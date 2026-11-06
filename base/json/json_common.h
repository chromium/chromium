// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_JSON_JSON_COMMON_H_
#define BASE_JSON_JSON_COMMON_H_

#include <stddef.h>

namespace base::json_internal {

// Chosen to support 99.9% of documents found in the wild late 2016.
// http://crbug.com/673263
// LINT.IfChange
inline constexpr size_t kAbsoluteMaxDepth = 200;
// LINT.ThenChange(//third_party/rust/serde_json_lenient/v0_2/wrapper/visitor.rs)

}  // namespace base::json_internal

#endif  // BASE_JSON_JSON_COMMON_H_
