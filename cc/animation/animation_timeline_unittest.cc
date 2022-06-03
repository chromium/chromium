// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_timeline.h"

#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/test/animation_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(AnimationTimelineTest, SyncAnimationsAttachDetach) {
  std::unique_ptr<AnimationHost> host(
      AnimationHost::CreateForTesting(ThreadInstance::MAIN));
  std::unique_ptr<AnimationHost> host_impl(
      AnimationHost::CreateForTesting(ThreadInstance::IMPL));

  const int timeline_id = AnimationIdProvider::NextTimelineId();
  const int animation_id = AnimationIdProvider::NextAnimationId();

  scoped_refptr<AnimationTimeline> timeline_impl(
      AnimationTimeline::Create(timeline_id));
  scoped_refptr<AnimationTimeline> timeline(
      AnimationTimeline::Create(timeline_id));

  host->AddAnimationTimeline(timeline.get());
  EXPECT_TRUE(timeline->animation_host());

  host_impl->AddAnimationTimeline(timeline_impl.get());
  EXPECT_TRUE(timeline_impl->animation_host());

  scoped_refptr<Animation> animation(Animation::Create(animation_id));
  timeline->AttachAnimation(animation.get());
  EXPECT_TRUE(animation->animation_timeline());

  EXPECT_FALSE(timeline_impl->GetAnimationById(animation_id));

  timeline->PushPropertiesTo(timeline_impl.get());

  scoped_refptr<Animation> animation_impl =
      timeline_impl->GetAnimationById(animation_id);
  EXPECT_TRUE(animation_impl);
  EXPECT_EQ(animation_impl->id(), animation_id);
  EXPECT_TRUE(animation_impl->animation_timeline());

  timeline->PushPropertiesTo(timeline_impl.get());
  EXPECT_EQ(animation_impl, timeline_impl->GetAnimationById(animation_id));

  timeline->DetachAnimation(animation.get());
  EXPECT_FALSE(animation->animation_timeline());

  timeline->PushPropertiesTo(timeline_impl.get());
  EXPECT_FALSE(timeline_impl->GetAnimationById(animation_id));

  EXPECT_FALSE(animation_impl->animation_timeline());
}

TEST(AnimationTimelineTest, ClearAnimations) {
  std::unique_ptr<AnimationHost> host(
      AnimationHost::CreateForTesting(ThreadInstance::MAIN));
  std::unique_ptr<AnimationHost> host_impl(
      AnimationHost::CreateForTesting(ThreadInstance::IMPL));

  const int timeline_id = AnimationIdProvider::NextTimelineId();
  const int animation_id1 = AnimationIdProvider::NextAnimationId();
  const int animation_id2 = AnimationIdProvider::NextAnimationId();

  scoped_refptr<AnimationTimeline> timeline_impl(
      AnimationTimeline::Create(timeline_id));
  scoped_refptr<AnimationTimeline> timeline(
      AnimationTimeline::Create(timeline_id));

  host->AddAnimationTimeline(timeline.get());
  host_impl->AddAnimationTimeline(timeline_impl.get());

  scoped_refptr<Animation> animation1(Animation::Create(animation_id1));
  timeline->AttachAnimation(animation1.get());
  scoped_refptr<Animation> animation2(Animation::Create(animation_id2));
  timeline->AttachAnimation(animation2.get());

  timeline->PushPropertiesTo(timeline_impl.get());

  EXPECT_TRUE(timeline_impl->GetAnimationById(animation_id1));
  EXPECT_TRUE(timeline_impl->GetAnimationById(animation_id2));

  timeline->ClearAnimations();
  EXPECT_FALSE(timeline->GetAnimationById(animation_id1));
  EXPECT_FALSE(timeline->GetAnimationById(animation_id2));

  timeline_impl->ClearAnimations();
  EXPECT_FALSE(timeline_impl->GetAnimationById(animation_id1));
  EXPECT_FALSE(timeline_impl->GetAnimationById(animation_id2));
}

}  // namespace
}  // namespace cc
