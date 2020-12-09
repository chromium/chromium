// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/document_transition/document_transition_request.h"

#include <utility>

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(DocumentTransitionRequestTest, PrepareRequest) {
  bool called = false;
  auto callback = base::BindLambdaForTesting([&called]() { called = true; });

  auto request = DocumentTransitionRequest::CreatePrepare(
      DocumentTransitionRequest::Effect::kRevealLeft,
      base::TimeDelta::FromMilliseconds(123), std::move(callback));

  EXPECT_FALSE(called);
  request->TakeCommitCallback().Run();
  EXPECT_TRUE(called);
  EXPECT_TRUE(request->TakeCommitCallback().is_null());

  auto directive = request->ConstructDirective();
  EXPECT_GT(directive.sequence_id(), 0u);
  EXPECT_EQ(DocumentTransitionRequest::Effect::kRevealLeft, directive.effect());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(123), directive.duration());
  EXPECT_EQ(viz::CompositorFrameTransitionDirective::Type::kSave,
            directive.type());

  auto duplicate = request->ConstructDirective();
  EXPECT_GT(duplicate.sequence_id(), directive.sequence_id());
  EXPECT_EQ(duplicate.effect(), directive.effect());
  EXPECT_EQ(duplicate.duration(), directive.duration());
  EXPECT_EQ(duplicate.type(), directive.type());
}

TEST(DocumentTransitionRequestTest, PrepareRequestLongDurationIsCapped) {
  auto long_duration = base::TimeDelta::FromSeconds(1);

  ASSERT_GT(long_duration,
            viz::CompositorFrameTransitionDirective::kMaxDuration);

  auto request = DocumentTransitionRequest::CreatePrepare(
      DocumentTransitionRequest::Effect::kRevealLeft, long_duration,
      base::OnceCallback<void()>());

  auto directive = request->ConstructDirective();
  EXPECT_EQ(viz::CompositorFrameTransitionDirective::kMaxDuration,
            directive.duration());
}

TEST(DocumentTransitionRequestTest, StartRequest) {
  bool called = false;
  auto callback = base::BindLambdaForTesting([&called]() { called = true; });

  auto request = DocumentTransitionRequest::CreateStart(std::move(callback));

  EXPECT_FALSE(called);
  request->TakeCommitCallback().Run();
  EXPECT_TRUE(called);
  EXPECT_TRUE(request->TakeCommitCallback().is_null());

  auto directive = request->ConstructDirective();
  EXPECT_GT(directive.sequence_id(), 0u);
  EXPECT_EQ(viz::CompositorFrameTransitionDirective::Type::kAnimate,
            directive.type());
}

}  // namespace cc
