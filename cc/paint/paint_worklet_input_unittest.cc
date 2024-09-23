// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_worklet_input.h"

#include "base/containers/flat_set.h"
#include "cc/test/test_paint_worklet_input.h"
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

// crbug.com/347682178
TEST(PaintWorkletInputTest, ValueChangeShouldCauseRepaint) {
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(100, 100));
  PaintWorkletInput::PropertyValue empty;
  EXPECT_FALSE(empty.has_value());

  PaintWorkletInput::PropertyValue float1(0.f);
  PaintWorkletInput::PropertyValue float2(1.f);
  EXPECT_FALSE(input->ValueChangeShouldCauseRepaint(float1, float1));
  EXPECT_TRUE(input->ValueChangeShouldCauseRepaint(float1, float2));

  PaintWorkletInput::PropertyValue color1(SkColors::kTransparent);
  PaintWorkletInput::PropertyValue color2(SkColors::kBlack);
  EXPECT_FALSE(input->ValueChangeShouldCauseRepaint(color1, color1));
  EXPECT_TRUE(input->ValueChangeShouldCauseRepaint(color1, color2));
}

}  // namespace cc
