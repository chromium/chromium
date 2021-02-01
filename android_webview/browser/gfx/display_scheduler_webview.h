// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_DISPLAY_SCHEDULER_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_DISPLAY_SCHEDULER_WEBVIEW_H_

#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/display_scheduler.h"

namespace android_webview {
class RootFrameSink;

class DisplaySchedulerWebView : public viz::DisplaySchedulerBase {
 public:
  DisplaySchedulerWebView(RootFrameSink* root_frame_sink);
  ~DisplaySchedulerWebView() override;

  // DisplaySchedulerBase implementation.
  void SetVisible(bool visible) override {}
  void ForceImmediateSwapIfPossible() override;
  void SetNeedsOneBeginFrame(bool needs_draw) override;
  void DidSwapBuffers() override;
  void DidReceiveSwapBuffersAck() override {}
  void OutputSurfaceLost() override;
  // DisplayDamageTrackerObserver implementation.
  void OnDisplayDamaged(viz::SurfaceId surface_id) override;
  void OnRootFrameMissing(bool missing) override {}
  void OnPendingSurfacesChanged() override {}

 private:
  RootFrameSink* const root_frame_sink_;

  // This count how many times specific sink damaged display. It's incremented
  // in OnDisplayDamaged and decremented in DidSwapBuffers.
  std::map<viz::FrameSinkId, int> damaged_frames_;

  THREAD_CHECKER(thread_checker_);
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_DISPLAY_SCHEDULER_WEBVIEW_H_
