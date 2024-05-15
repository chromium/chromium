// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_compositor_frame_reporting_controller.h"

#include <utility>
#include <vector>

#include "components/viz/common/frame_timing_details.h"

namespace cc {
base::TimeDelta INTERVAL = base::Milliseconds(16);

FakeCompositorFrameReportingController::FakeCompositorFrameReportingController()
    : CompositorFrameReportingController(/*should_report_histograms=*/false,
                                         /*should_report_ukm=*/false,
                                         /*layer_tree_host_id=*/1) {}

void FakeCompositorFrameReportingController::WillBeginMainFrame(
    const viz::BeginFrameArgs& args) {
  if (!HasReporterAt(PipelineStage::kBeginImplFrame))
    CompositorFrameReportingController::WillBeginImplFrame(args);
  CompositorFrameReportingController::WillBeginMainFrame(args);
}

void FakeCompositorFrameReportingController::BeginMainFrameAborted(
    const viz::BeginFrameId& id,
    CommitEarlyOutReason reason) {
  if (!HasReporterAt(PipelineStage::kBeginMainFrame)) {
    viz::BeginFrameArgs args = viz::BeginFrameArgs();
    args.frame_id = id;
    args.frame_time = Now();
    args.interval = INTERVAL;
    WillBeginMainFrame(args);
  }
  CompositorFrameReportingController::BeginMainFrameAborted(id, reason);
}

void FakeCompositorFrameReportingController::WillCommit() {
  if (!HasReporterAt(PipelineStage::kReadyToCommit)) {
    if (!HasReporterAt(PipelineStage::kBeginMainFrame)) {
      viz::BeginFrameArgs args = viz::BeginFrameArgs();
      args.frame_id = viz::BeginFrameId();
      args.frame_time = Now();
      args.interval = INTERVAL;
      WillBeginMainFrame(args);
    }
    NotifyReadyToCommit(nullptr);
  }
  CompositorFrameReportingController::WillCommit();
}

void FakeCompositorFrameReportingController::DidCommit() {
  if (!HasReporterAt(PipelineStage::kBeginMainFrame))
    WillCommit();
  CompositorFrameReportingController::DidCommit();
}

void FakeCompositorFrameReportingController::WillActivate() {
  // Pending trees for impl-side invalidations are created without a prior
  // commit.
  if (!HasReporterAt(PipelineStage::kCommit) &&
      !next_activate_has_invalidation())
    DidCommit();
  CompositorFrameReportingController::WillActivate();
}

void FakeCompositorFrameReportingController::DidActivate() {
  // Pending trees for impl-side invalidations are created without a prior
  // commit.
  if (!HasReporterAt(PipelineStage::kCommit) &&
      !next_activate_has_invalidation())
    WillActivate();
  CompositorFrameReportingController::DidActivate();
}

void FakeCompositorFrameReportingController::DidSubmitCompositorFrame(
    SubmitInfo& submit_info,
    const viz::BeginFrameId& current_frame_id,
    const viz::BeginFrameId& last_activated_frame_id) {
  CompositorFrameReportingController::DidSubmitCompositorFrame(
      submit_info, current_frame_id, last_activated_frame_id);

  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = base::TimeTicks::Now();
  CompositorFrameReportingController::DidPresentCompositorFrame(
      submit_info.frame_token, details);
}

void FakeCompositorFrameReportingController::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {}
}  // namespace cc
