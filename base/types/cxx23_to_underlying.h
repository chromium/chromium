// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_CXX23_TO_UNDERLYING_H_
#define BASE_TYPES_CXX23_TO_UNDERLYING_H_

#include <utility>

namespace base {

// TODO(crbug.com/470039537): Migrate usages and delete this alias.
using std::to_underlying;

}  // namespace base

#endif  // BASE_TYPES_CXX23_TO_UNDERLYING_H_
