// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_DISPLAY_SCHEDULER_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_DISPLAY_SCHEDULER_WEBVIEW_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/display_scheduler.h"

namespace android_webview {
class RootFrameSink;

class OverlaysInfoProvider {
 public:
  virtual bool IsFrameSinkOverlayed(viz::FrameSinkId frame_sink_id) = 0;
};

class DisplaySchedulerWebView : public viz::DisplaySchedulerBase {
 public:
  DisplaySchedulerWebView(RootFrameSink* root_frame_sink,
                          OverlaysInfoProvider* overlays_info_provider);
  ~DisplaySchedulerWebView() override;

  // DisplaySchedulerBase implementation.
  void SetVisible(bool visible) override {}
  void ForceImmediateSwapIfPossible() override;
  void SetNeedsOneBeginFrame(bool needs_draw) override;
  void DidSwapBuffers() override;
  void DidReceiveSwapBuffersAck() override {}
  void OutputSurfaceLost() override;
  void ReportFrameTime(
      base::TimeDelta frame_time,
      base::flat_set<base::PlatformThreadId> thread_ids) override {}

  // DisplayDamageTrackerObserver implementation.
  void OnDisplayDamaged(viz::SurfaceId surface_id) override;
  void OnRootFrameMissing(bool missing) override {}
  void OnPendingSurfacesChanged() override {}

 private:
  bool IsFrameSinkOverlayed(viz::FrameSinkId frame_sink_id);

  const raw_ptr<RootFrameSink> root_frame_sink_;

  // This count how many times specific sink damaged display. It's incremented
  // in OnDisplayDamaged and decremented in DidSwapBuffers.
  std::map<viz::FrameSinkId, int> damaged_frames_;

  // Due to destruction order in viz::Display this might be not safe to use in
  // destructor of this class.
  const raw_ptr<OverlaysInfoProvider> overlays_info_provider_;

  THREAD_CHECKER(thread_checker_);
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_DISPLAY_SCHEDULER_WEBVIEW_H_
