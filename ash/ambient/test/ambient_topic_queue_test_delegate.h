// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_AMBIENT_TOPIC_QUEUE_TEST_DELEGATE_H_
#define ASH_AMBIENT_TEST_AMBIENT_TOPIC_QUEUE_TEST_DELEGATE_H_

#include "ash/ambient/model/ambient_topic_queue.h"

namespace ash {

// Returns a single topic size by default. Tests can override with a custom set
// if desired.
class AmbientTopicQueueTestDelegate : public AmbientTopicQueue::Delegate {
 public:
  AmbientTopicQueueTestDelegate();
  AmbientTopicQueueTestDelegate(const AmbientTopicQueueTestDelegate&) = delete;
  AmbientTopicQueueTestDelegate& operator=(
      const AmbientTopicQueueTestDelegate&) = delete;
  ~AmbientTopicQueueTestDelegate() override;

  // AmbientTopicQueue::Delegate implementation:
  std::vector<gfx::Size> GetTopicSizes() override;

  void SetTopicSizes(std::vector<gfx::Size> topic_sizes);

 private:
  std::vector<gfx::Size> topic_sizes_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_TOPIC_QUEUE_TEST_DELEGATE_H_
