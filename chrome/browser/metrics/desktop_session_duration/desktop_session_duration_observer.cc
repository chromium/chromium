// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "content/public/browser/render_view_host.h"

namespace metrics {

DesktopSessionDurationObserver::DesktopSessionDurationObserver(
    content::WebContents* web_contents,
    DesktopSessionDurationTracker* service)
    : content::WebContentsObserver(web_contents), service_(service) {
  RegisterInputEventObserver(web_contents->GetPrimaryMainFrame());
}

DesktopSessionDurationObserver::~DesktopSessionDurationObserver() {
  if (web_contents()) {
    UnregisterInputEventObserver(web_contents()->GetPrimaryMainFrame());
  }
}

// static
std::unique_ptr<DesktopSessionDurationObserver>
DesktopSessionDurationObserver::MaybeCreate(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  if (!DesktopSessionDurationTracker::IsInitialized()) {
    return nullptr;
  }

  return std::make_unique<DesktopSessionDurationObserver>(
      web_contents, DesktopSessionDurationTracker::Get());
}

void DesktopSessionDurationObserver::RegisterInputEventObserver(
    content::RenderFrameHost* host) {
  if (host != nullptr)
    host->GetRenderWidgetHost()->AddInputEventObserver(this);
}

void DesktopSessionDurationObserver::UnregisterInputEventObserver(
    content::RenderFrameHost* host) {
  if (host != nullptr)
    host->GetRenderWidgetHost()->RemoveInputEventObserver(this);
}

void DesktopSessionDurationObserver::OnInputEvent(
    const content::RenderWidgetHost& widget,
    const blink::WebInputEvent& event,
    input::InputEventSource source) {
  service_->OnUserEvent(event.GetTypeAsUiEventType());
}

void DesktopSessionDurationObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (!new_host->IsInPrimaryMainFrame())
    return;

  UnregisterInputEventObserver(old_host);
  RegisterInputEventObserver(new_host);
}

}  // namespace metrics
