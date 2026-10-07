// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_id_provider.h"

#include <limits>

#include "base/atomic_sequence_num.h"
#include "base/check_op.h"

namespace cc {

namespace {

int GetNextId(base::AtomicSequenceNumber& seq) {
  // Animation IDs start from 1.
  int prev = seq.GetNext();
  CHECK_GE(prev, 0);
  CHECK_LT(prev, std::numeric_limits<int>::max());
  return prev + 1;
}

}  // namespace

base::AtomicSequenceNumber g_next_keyframe_model_id;
base::AtomicSequenceNumber g_next_group_id;
base::AtomicSequenceNumber g_next_timeline_id;
base::AtomicSequenceNumber g_next_animation_id;
base::AtomicSequenceNumber g_next_animation_trigger_id;

int AnimationIdProvider::NextKeyframeModelId() {
  return GetNextId(g_next_keyframe_model_id);
}

int AnimationIdProvider::NextGroupId() {
  return GetNextId(g_next_group_id);
}

int AnimationIdProvider::NextTimelineId() {
  return GetNextId(g_next_timeline_id);
}

int AnimationIdProvider::NextAnimationId() {
  return GetNextId(g_next_animation_id);
}

int AnimationIdProvider::NextAnimationTriggerId() {
  return GetNextId(g_next_animation_trigger_id);
}

}  // namespace cc
