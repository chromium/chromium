// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_HELPERS_PAGE_LIVE_STATE_DECORATOR_HELPER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_HELPERS_PAGE_LIVE_STATE_DECORATOR_HELPER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/glic/public/glic_perf_traits_tracker.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "components/performance_manager/public/performance_manager_observer.h"
#include "content/public/browser/devtools_agent_host.h"

namespace performance_manager {

namespace {
class ActiveTabObserver;
}

class PageLiveStateDecoratorHelper
    : public MediaStreamCaptureIndicator::Observer,
      public PerformanceManagerObserver,
      public content::DevToolsAgentHostObserver,
      public glic::GlicPerfTraitsTracker::Observer {
 public:
  PageLiveStateDecoratorHelper();
  ~PageLiveStateDecoratorHelper() override;
  PageLiveStateDecoratorHelper(const PageLiveStateDecoratorHelper& other) =
      delete;
  PageLiveStateDecoratorHelper& operator=(const PageLiveStateDecoratorHelper&) =
      delete;

  // MediaStreamCaptureIndicator::Observer implementation:
  void OnIsCapturingVideoChanged(content::WebContents* contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* contents,
                                 bool is_capturing_audio) override;
  void OnIsBeingMirroredChanged(content::WebContents* contents,
                                bool is_being_mirrored) override;
  void OnIsCapturingTabChanged(content::WebContents* contents,
                               bool is_capturing_tab) override;
  void OnIsCapturingWindowChanged(content::WebContents* contents,
                                  bool is_capturing_window) override;
  void OnIsCapturingDisplayChanged(content::WebContents* contents,
                                   bool is_capturing_display) override;

  // content::DevToolsAgentHostObserver:
  void DevToolsAgentHostAttached(
      content::DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(
      content::DevToolsAgentHost* agent_host) override;

  // PerformanceManagerObserver:
  void OnPageNodeCreatedForWebContents(
      content::WebContents* web_contents) override;

  // glic::GlicPerfTraitsTracker::Observer:
  void OnGlicActuationStateChanged(content::WebContents* web_contents,
                                   GlicActuationState state) override;
  void OnIsGlicPinnedToVisibleInstanceChanged(
      content::WebContents* web_contents,
      bool is_pinned_to_visible) override;

 private:
  class WebContentsObserver;

  // Linked list of WebContentsObservers created by this
  // PageLiveStateDecoratorHelper. Each WebContentsObservers removes itself from
  // the list and destroys itself when its associated WebContents is destroyed.
  // Additionally, all WebContentsObservers that are still in this list when the
  // destructor of PageLiveStateDecoratorHelper is invoked are destroyed.
  raw_ptr<WebContentsObserver> first_web_contents_observer_ = nullptr;

  std::unique_ptr<ActiveTabObserver> active_tab_observer_;

  base::ScopedObservation<glic::GlicPerfTraitsTracker,
                          glic::GlicPerfTraitsTracker::Observer>
      glic_perf_traits_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_HELPERS_PAGE_LIVE_STATE_DECORATOR_HELPER_H_
