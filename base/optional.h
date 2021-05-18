// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OPTIONAL_H_
#define BASE_OPTIONAL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// These aliases are deprecated. Use abseil directly instead.

template <typename T>
using Optional [[deprecated]] = absl::optional<T>;

using absl::make_optional;
using absl::nullopt;
using absl::nullopt_t;

}  // namespace base

#endif  // BASE_OPTIONAL_H_
