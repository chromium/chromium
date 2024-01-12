// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gtest_prod_util.h"

namespace base::internal {

namespace {

InDeathTestChildFn g_in_death_test_fn = nullptr;

}

bool InDeathTestChild() {
  return g_in_death_test_fn && (*g_in_death_test_fn)();
}

void SetInDeathTestChildFn(InDeathTestChildFn in_death_test_child_fn) {
  g_in_death_test_fn = in_death_test_child_fn;
}

}  // namespace base::internal
