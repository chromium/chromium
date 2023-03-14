// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/tile_task.h"

#include <utility>

#include "base/check.h"

namespace cc {

TileTask::TileTask(
    SupportsConcurrentExecution supports_concurrent_execution,
    SupportsBackgroundThreadPriority supports_background_thread_priority,
    TileTask::Vector* dependencies)
    : supports_concurrent_execution_(supports_concurrent_execution),
      supports_background_thread_priority_(supports_background_thread_priority),
      dependencies_(dependencies ? std::move(*dependencies)
                                 : TileTask::Vector()),
      did_complete_(false) {}

TileTask::~TileTask() {
  DCHECK(did_complete_);
}

void TileTask::DidComplete() {
  DCHECK(!did_complete_);
  did_complete_ = true;
}

bool TileTask::HasCompleted() const {
  return did_complete_;
}

bool TileTask::TaskContainsLCPCandidateImages() const {
  for (auto dependent : dependencies_) {
    if (!dependent->HasCompleted() &&
        dependent->TaskContainsLCPCandidateImages()) {
      return true;
    }
  }
  return false;
}

}  // namespace cc
