// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_worklet_input.h"

#include "base/containers/flat_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(PaintWorkletInputTest, InsertPropertyKeyToFlatSet) {
  base::flat_set<PaintWorkletInput::PropertyKey> property_keys;

  PaintWorkletInput::PropertyKey key1("foo", ElementId(128u));
  property_keys.insert(key1);
  EXPECT_EQ(property_keys.size(), 1u);

  PaintWorkletInput::PropertyKey key2("foo", ElementId(130u));
  property_keys.insert(key2);
  EXPECT_EQ(property_keys.size(), 2u);
}

}  // namespace cc
