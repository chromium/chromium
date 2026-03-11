// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_scroll_observer.h"

#include "components/send_tab_to_self/metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"

namespace send_tab_to_self {

SendTabToSelfScrollObserver::SendTabToSelfScrollObserver(
    content::WebContents* web_contents,
    bool restoration_attempted)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SendTabToSelfScrollObserver>(*web_contents),
      restoration_attempted_(restoration_attempted) {}

SendTabToSelfScrollObserver::~SendTabToSelfScrollObserver() {
  RecordMetrics();
  if (observed_host_) {
    observed_host_->RemoveInputEventObserver(this);
    observed_host_->RemoveObserver(this);
  }
}

void SendTabToSelfScrollObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!observed_host_) {
    content::RenderWidgetHost* host =
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
    if (host) {
      host->AddInputEventObserver(this);
      host->AddObserver(this);
      observed_host_ = host;
    }
  } else {
    // Navigated away, record what we have and stop.
    RecordMetrics();
    web_contents()->RemoveUserData(UserDataKey());
  }
}

void SendTabToSelfScrollObserver::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  if (observed_host_ == widget_host) {
    observed_host_->RemoveInputEventObserver(this);
    observed_host_->RemoveObserver(this);
    observed_host_ = nullptr;
  }
}

void SendTabToSelfScrollObserver::WebContentsDestroyed() {
  RecordMetrics();
  web_contents()->RemoveUserData(UserDataKey());
}

void SendTabToSelfScrollObserver::OnInputEvent(
    const content::RenderWidgetHost& host,
    const blink::WebInputEvent& event,
    input::InputEventSource source) {
  // We use kGestureScrollUpdate because it represents the actual pixel
  // displacement on the page, after various input sources (mouse wheel, touch,
  // etc.) have been processed and normalized.
  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate) {
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(event);
    total_scroll_volume_ += std::abs(gesture_event.data.scroll_update.delta_y);
  }
}

void SendTabToSelfScrollObserver::RecordMetrics() {
  if (metrics_recorded_) {
    return;
  }
  RecordScrollVolume(total_scroll_volume_, restoration_attempted_);
  metrics_recorded_ = true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SendTabToSelfScrollObserver);

}  // namespace send_tab_to_self
