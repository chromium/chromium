// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gtest_prod_util.h"

namespace base::internal {

namespace {

bool g_in_death_test = false;
}

bool InDeathTestChild() {
  return g_in_death_test;
}

void SetInDeathTestChildForTesting(bool in_death_test_child) {
  g_in_death_test = in_death_test_child;
}

}  // namespace base::internal
