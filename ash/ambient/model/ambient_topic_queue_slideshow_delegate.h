// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_TOPIC_QUEUE_SLIDESHOW_DELEGATE_H_
#define ASH_AMBIENT_MODEL_AMBIENT_TOPIC_QUEUE_SLIDESHOW_DELEGATE_H_

#include "ash/ambient/model/ambient_topic_queue.h"
#include "ash/ash_export.h"

namespace ash {

// For the UI that iterates through a slideshow of images and displays them at
// full-screen resolution.
class ASH_EXPORT AmbientTopicQueueSlideshowDelegate
    : public AmbientTopicQueue::Delegate {
 public:
  AmbientTopicQueueSlideshowDelegate();
  AmbientTopicQueueSlideshowDelegate(
      const AmbientTopicQueueSlideshowDelegate&) = delete;
  AmbientTopicQueueSlideshowDelegate& operator=(
      const AmbientTopicQueueSlideshowDelegate&) = delete;
  ~AmbientTopicQueueSlideshowDelegate() override;

  // AmbientTopicQueue::Delegate implementation:
  std::vector<gfx::Size> GetTopicSizes() override;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_TOPIC_QUEUE_SLIDESHOW_DELEGATE_H_
