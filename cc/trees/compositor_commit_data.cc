// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/compositor_commit_data.h"

#include "cc/trees/swap_promise.h"

namespace cc {

CompositorCommitData::CompositorCommitData() = default;

CompositorCommitData::~CompositorCommitData() = default;

CompositorCommitData::ScrollUpdateInfo::ScrollUpdateInfo() = default;

CompositorCommitData::ScrollUpdateInfo::ScrollUpdateInfo(
    ElementId id,
    gfx::Vector2dF delta,
    std::optional<TargetSnapAreaElementIds> snap_target_ids)
    : element_id(id),
      scroll_delta(delta),
      snap_target_element_ids(snap_target_ids) {}

CompositorCommitData::ScrollUpdateInfo::ScrollUpdateInfo(
    const ScrollUpdateInfo& other) = default;

CompositorCommitData::ScrollUpdateInfo&
CompositorCommitData::ScrollUpdateInfo::operator=(
    const ScrollUpdateInfo& other) = default;

}  // namespace cc
