// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CXX17_BACKPORTS_H_
#define BASE_CXX17_BACKPORTS_H_

#include <algorithm>

namespace base {

// TODO(crbug.com/1373621): Rewrite all uses of base::clamp as std::clamp and
// remove this file.
using std::clamp;

}  // namespace base

#endif  // BASE_CXX17_BACKPORTS_H_
