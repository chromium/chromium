// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_TOPIC_QUEUE_ANIMATION_DELEGATE_H_
#define ASH_AMBIENT_MODEL_AMBIENT_TOPIC_QUEUE_ANIMATION_DELEGATE_H_

#include <vector>

#include "ash/ambient/model/ambient_topic_queue.h"
#include "ash/ash_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class SkottieResourceMetadataMap;
}  // namespace cc

namespace ash {

// For the UI that renders a Lottie animation file using the Skottie library.
class ASH_EXPORT AmbientTopicQueueAnimationDelegate
    : public AmbientTopicQueue::Delegate {
 public:
  explicit AmbientTopicQueueAnimationDelegate(
      const cc::SkottieResourceMetadataMap& resource_metadata);
  AmbientTopicQueueAnimationDelegate(
      const AmbientTopicQueueAnimationDelegate&) = delete;
  AmbientTopicQueueAnimationDelegate& operator=(
      const AmbientTopicQueueAnimationDelegate&) = delete;
  ~AmbientTopicQueueAnimationDelegate() override;

  // AmbientTopicQueue::Delegate implementation:
  std::vector<gfx::Size> GetTopicSizes() override;

 private:
  const std::vector<gfx::Size> topic_sizes_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_TOPIC_QUEUE_ANIMATION_DELEGATE_H_
