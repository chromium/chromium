// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/enqueue_order_generator.h"

namespace base::sequence_manager::internal {

EnqueueOrderGenerator::EnqueueOrderGenerator()
    : counter_(EnqueueOrder::kFirst) {}

EnqueueOrderGenerator::~EnqueueOrderGenerator() = default;

}  // namespace base::sequence_manager::internal
