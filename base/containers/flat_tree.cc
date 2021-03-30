// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/flat_tree.h"

// flat_tree.h is a widely included header and its size has significant impact
// on build time. Try not to raise this limit unless absolutely necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#ifndef NACL_TC_REV
#pragma clang max_tokens_here 260000
#endif

namespace base {

sorted_unique_t sorted_unique;

}  // namespace base
