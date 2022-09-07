// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/rust/tests/test_rs_bindings_from_cc/self_contained_target_header2.h"

int AddViaCc(int x, int y) {
  return x + y;
}
