// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/tile_task.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "cc/base/features.h"

namespace cc {

TileTask::TileTask(
    SupportsConcurrentExecution supports_concurrent_execution,
    SupportsBackgroundThreadPriority supports_background_thread_priority,
    TileTask::Vector* dependencies)
    : supports_concurrent_execution_(supports_concurrent_execution),
      supports_background_thread_priority_(supports_background_thread_priority),
      dependencies_(dependencies ? std::move(*dependencies)
                                 : TileTask::Vector()) {}

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

bool TileTask::IsRasterTask() const {
  return true;
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

void TileTask::SetExternalDependent(scoped_refptr<TileTask> dependent) {
  if (base::FeatureList::IsEnabled(features::kPreventDuplicateImageDecodes)) {
    CHECK(IsRasterTask() != dependent->IsRasterTask());
    // A task may have at most one external dependent.
    CHECK(!external_dependent_ || external_dependent_->state().IsCanceled());
    // A task may have at most one external dependency, and may not mix internal
    // and external dependencies.
    CHECK_EQ(dependent->dependencies_.size(), 0u);
    dependent->dependencies_.push_back(this);
    external_dependent_ = std::move(dependent);
  }
}

void TileTask::ExternalDependencyCompleted() {
  CHECK_EQ(dependencies_.size(), 1u);
  dependencies_.clear();
}

}  // namespace cc
