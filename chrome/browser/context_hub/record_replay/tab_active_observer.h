// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_TAB_ACTIVE_OBSERVER_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_TAB_ACTIVE_OBSERVER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents_observer.h"

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace context_hub {

class SkillRecordingService;

// Helper class to observe user interactions on the currently active tab's
// `WebContents`.
class TabActiveObserver final
    : public content::WebContentsObserver,
      public content::RenderWidgetHost::InputEventObserver,
      public content::RenderWidgetHostObserver {
 public:
  // `service` and `web_contents` must be non-null and must outlive this
  // observer.
  TabActiveObserver(SkillRecordingService* service,
                    content::WebContents* web_contents);

  TabActiveObserver(const TabActiveObserver&) = delete;
  TabActiveObserver& operator=(const TabActiveObserver&) = delete;

  ~TabActiveObserver() override;

 private:
  // Populated in `OnInputEvent()` and consumed synchronously in
  // `DidGetUserInteraction()` on the same `WebContentsImpl::OnInputEvent()`
  // call stack, matching the routed `WebInputEvent` by `TimeStamp()`.
  struct PendingMouseDown {
    content::GlobalRenderFrameHostToken frame_token;
    base::TimeTicks timestamp;
  };

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const content::RenderWidgetHost& host,
                    const blink::WebInputEvent& event,
                    InputEventSource source) override;

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  void MaybeObserveRenderWidgetHost(
      content::RenderFrameHost* render_frame_host);

  const raw_ref<SkillRecordingService> service_;
  std::optional<PendingMouseDown> pending_mouse_down_;
  base::ScopedMultiSourceObservation<
      content::RenderWidgetHost,
      content::RenderWidgetHost::InputEventObserver>
      input_event_observations_{this};
  base::ScopedMultiSourceObservation<content::RenderWidgetHost,
                                     content::RenderWidgetHostObserver>
      widget_host_observations_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_TAB_ACTIVE_OBSERVER_H_
