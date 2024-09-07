// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_common.h"

#include "base/trace_event/trace_id_helper.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/mutator_host.h"

namespace cc {

BeginMainFrameAndCommitState::BeginMainFrameAndCommitState()
    : trace_id(base::trace_event::GetNextGlobalTraceId()) {}

BeginMainFrameAndCommitState::~BeginMainFrameAndCommitState() = default;

}  // namespace cc
