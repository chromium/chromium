// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_JSON_JSON_COMMON_H_
#define BASE_JSON_JSON_COMMON_H_

#include <stddef.h>

namespace base::internal {

// Chosen to support 99.9% of documents found in the wild late 2016.
// http://crbug.com/673263
inline constexpr size_t kAbsoluteMaxDepth = 200;

}  // namespace base::internal

#endif  // BASE_JSON_JSON_COMMON_H_
