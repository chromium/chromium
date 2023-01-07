// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_topic_queue_test_delegate.h"

#include <utility>

#include "base/check.h"

namespace ash {

AmbientTopicQueueTestDelegate::AmbientTopicQueueTestDelegate()
    : topic_sizes_({gfx::Size(500, 500)}) {}

AmbientTopicQueueTestDelegate::~AmbientTopicQueueTestDelegate() = default;

std::vector<gfx::Size> AmbientTopicQueueTestDelegate::GetTopicSizes() {
  return topic_sizes_;
}

void AmbientTopicQueueTestDelegate::SetTopicSizes(
    std::vector<gfx::Size> topic_sizes) {
  CHECK(!topic_sizes.empty());
  for (const gfx::Size& topic_size : topic_sizes) {
    CHECK(!topic_size.IsEmpty());
  }
  topic_sizes_ = std::move(topic_sizes);
}

}  // namespace ash
