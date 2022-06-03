// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/mock_mutator_host.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(TestMockMutatorHost, ConstructAndPoke) {
  MockMutatorHost mock;
  EXPECT_CALL(mock, NextFrameHasPendingRAF).Times(1);
  mock.NextFrameHasPendingRAF();
}

}  // namespace cc
