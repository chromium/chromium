// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/test/test_begin_frame_source.h"
#include "base/check.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"

namespace ash {

TestBeginFrameSource::TestBeginFrameSource()
    : viz::BeginFrameSource(kNotRestartableId) {}
TestBeginFrameSource::~TestBeginFrameSource() = default;

void TestBeginFrameSource::DidFinishFrame(viz::BeginFrameObserver* obs) {}

void TestBeginFrameSource::AddObserver(viz::BeginFrameObserver* obs) {
  DCHECK(obs);
  observer_ = obs;
}
void TestBeginFrameSource::RemoveObserver(viz::BeginFrameObserver* obs) {
  DCHECK_EQ(observer_, obs);
  observer_ = nullptr;
}

void TestBeginFrameSource::OnGpuNoLongerBusy() {}

viz::BeginFrameObserver* TestBeginFrameSource::GetBeginFrameObserver() const {
  return observer_;
}

viz::BeginFrameArgs CreateValidBeginFrameArgsForTesting() {
  auto interval = base::Milliseconds(16);
  base::TimeTicks now = base::TimeTicks::Now();
  auto deadline = now + interval;

  viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 1u, /*sequence_number=*/2u, now, deadline, interval,
      viz::BeginFrameArgs::NORMAL);

  return args;
}

}  // namespace ash
