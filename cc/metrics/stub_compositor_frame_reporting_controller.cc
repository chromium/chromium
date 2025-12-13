// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/stub_compositor_frame_reporting_controller.h"

#include <memory>

namespace cc {

StubCompositorFrameReportingController::StubCompositorFrameReportingController()
    : CompositorFrameReportingController(/*should_report_histograms=*/false,
                                         /*should_report_ukm=*/false,
                                         /*layer_tree_host_id=*/1,
                                         /*is_trees_in_viz_client=*/false) {}

void StubCompositorFrameReportingController::NotifyReadyToCommit(
    std::unique_ptr<BeginMainFrameMetrics> details) {}

void StubCompositorFrameReportingController::WillBeginMainFrame(
    const viz::BeginFrameArgs& args) {}

void StubCompositorFrameReportingController::BeginMainFrameAborted(
    const viz::BeginFrameId& id,
    CommitEarlyOutReason reason) {}

void StubCompositorFrameReportingController::WillCommit() {}

void StubCompositorFrameReportingController::DidCommit() {}

void StubCompositorFrameReportingController::WillActivate() {}

void StubCompositorFrameReportingController::DidActivate() {}

void StubCompositorFrameReportingController::DidSubmitCompositorFrame(
    SubmitInfo& submit_info,
    const viz::BeginFrameId& current_frame_id,
    const viz::BeginFrameId& last_activated_frame_id) {}

void StubCompositorFrameReportingController::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {}

}  // namespace cc
