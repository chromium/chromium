// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojom/browser_controls_snap_hint_mojom_traits.h"

#include <limits>

#include "cc/input/browser_controls_snap_hint.h"
#include "cc/mojom/browser_controls_snap_hint.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

bool RoundTrip(const BrowserControlsSnapHint& in,
               BrowserControlsSnapHint* out) {
  return mojo::test::SerializeAndDeserialize<mojom::BrowserControlsSnapHint>(
      in, *out);
}

TEST(BrowserControlsSnapHintMojomTraitsTest, RoundTripPreservesAllFields) {
  for (BrowserControlsSnapHint::Action action :
       {BrowserControlsSnapHint::Action::kMustShow,
        BrowserControlsSnapHint::Action::kCanShow,
        BrowserControlsSnapHint::Action::kCanHide}) {
    const BrowserControlsSnapHint in{action, /*slowness=*/0.25f,
                                     /*is_scroll_end_or_inertial=*/true};
    BrowserControlsSnapHint out;
    ASSERT_TRUE(RoundTrip(in, &out));
    EXPECT_EQ(in.action, out.action);
    EXPECT_EQ(in.slowness, out.slowness);
    EXPECT_EQ(in.is_scroll_end_or_inertial, out.is_scroll_end_or_inertial);
  }
}

TEST(BrowserControlsSnapHintMojomTraitsTest, AcceptsSlownessBounds) {
  BrowserControlsSnapHint out;
  for (float slowness : {0.0f, 1.0f}) {
    const BrowserControlsSnapHint in{BrowserControlsSnapHint::Action::kCanHide,
                                     slowness,
                                     /*is_scroll_end_or_inertial=*/false};
    EXPECT_TRUE(RoundTrip(in, &out)) << "slowness=" << slowness;
  }
}

TEST(BrowserControlsSnapHintMojomTraitsTest, RejectsSlownessOutsideUnitRange) {
  // The traits CHECK when they serialize an invalid slowness, so start from
  // the wire-format struct.
  for (float slowness : {-0.1f, 1.1f, std::numeric_limits<float>::quiet_NaN(),
                         std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity()}) {
    mojom::BrowserControlsSnapHintPtr in = mojom::BrowserControlsSnapHint::New(
        BrowserControlsSnapHint::Action::kCanHide, slowness,
        /*is_scroll_end_or_inertial=*/false);
    BrowserControlsSnapHint out;
    EXPECT_FALSE(
        mojo::test::SerializeAndDeserialize<mojom::BrowserControlsSnapHint>(
            in, out))
        << "slowness=" << slowness;
  }
}

}  // namespace
}  // namespace cc
