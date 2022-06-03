// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_interface_impl.h"

namespace base {

TestInterfaceImpl::TestInterfaceImpl() = default;
TestInterfaceImpl::~TestInterfaceImpl() = default;

void TestInterfaceImpl::Add(int32_t a, int32_t b, AddCallback callback) {
  callback(a + b);
}

}  // namespace base
