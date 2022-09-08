// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/atomic_sequence_num.h"
#include "cc/animation/animation_id_provider.h"

namespace cc {

base::AtomicSequenceNumber g_next_keyframe_model_id;
base::AtomicSequenceNumber g_next_group_id;
base::AtomicSequenceNumber g_next_timeline_id;
base::AtomicSequenceNumber g_next_animation_id;

int AnimationIdProvider::NextKeyframeModelId() {
  // Animation IDs start from 1.
  return g_next_keyframe_model_id.GetNext() + 1;
}

int AnimationIdProvider::NextGroupId() {
  // Animation group IDs start from 1.
  return g_next_group_id.GetNext() + 1;
}

int AnimationIdProvider::NextTimelineId() {
  return g_next_timeline_id.GetNext() + 1;
}

int AnimationIdProvider::NextAnimationId() {
  return g_next_animation_id.GetNext() + 1;
}

}  // namespace cc
