// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/view_transition/view_transition_request.h"

#include <utility>

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(ViewTransitionRequestTest, PrepareRequest) {
  bool called = false;
  auto callback = base::BindLambdaForTesting([&called]() { called = true; });

  auto request = ViewTransitionRequest::CreateCapture(
      blink::ViewTransitionToken(),
      /*maybe_cross_frame_sink=*/false, {}, std::move(callback));

  EXPECT_FALSE(called);
  request->TakeFinishedCallback().Run();
  EXPECT_TRUE(called);
  EXPECT_TRUE(request->TakeFinishedCallback().is_null());

  auto directive = request->ConstructDirective({}, {});
  EXPECT_GT(directive.sequence_id(), 0u);
  EXPECT_EQ(viz::CompositorFrameTransitionDirective::Type::kSave,
            directive.type());

  auto duplicate = request->ConstructDirective({}, {});
  EXPECT_EQ(duplicate.sequence_id(), directive.sequence_id());
  EXPECT_EQ(duplicate.type(), directive.type());
}

TEST(ViewTransitionRequestTest, StartRequest) {
  auto request = ViewTransitionRequest::CreateAnimateRenderer(
      blink::ViewTransitionToken(),
      /*maybe_cross_frame_sink=*/false);

  EXPECT_TRUE(request->TakeFinishedCallback().is_null());

  auto directive = request->ConstructDirective({}, {});
  EXPECT_GT(directive.sequence_id(), 0u);
  EXPECT_EQ(viz::CompositorFrameTransitionDirective::Type::kAnimateRenderer,
            directive.type());
}

}  // namespace cc
