// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_STUB_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
#define CC_METRICS_STUB_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_

#include <memory>

#include "cc/metrics/compositor_frame_reporting_controller.h"

namespace cc {

class StubCompositorFrameReportingController
    : public CompositorFrameReportingController {
 public:
  StubCompositorFrameReportingController();

  StubCompositorFrameReportingController(
      const StubCompositorFrameReportingController& controller) = delete;
  StubCompositorFrameReportingController& operator=(
      const StubCompositorFrameReportingController& controller) = delete;

  void NotifyReadyToCommit(
      std::unique_ptr<BeginMainFrameMetrics> details) override;
  void WillBeginMainFrame(const viz::BeginFrameArgs& args) override;
  void BeginMainFrameAborted(
      const viz::BeginFrameId& id,
      CommitEarlyOutReason reason =
          CommitEarlyOutReason::kAbortedNotVisible) override;
  void WillCommit() override;
  void DidCommit() override;
  void WillActivate() override;
  void DidActivate() override;
  void DidSubmitCompositorFrame(
      SubmitInfo& submit_info,
      const viz::BeginFrameId& current_frame_id,
      const viz::BeginFrameId& last_activated_frame_id) override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override;
};

}  // namespace cc

#endif  // CC_METRICS_STUB_COMPOSITOR_FRAME_REPORTING_CONTROLLER_H_
