// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/tab_active_observer.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/context_hub/record_replay/ax_click_target_resolver.h"
#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

bool IsLocalRootFrame(content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host) {
    return false;
  }
  content::RenderFrameHost* parent = render_frame_host->GetParent();
  return !parent || render_frame_host->GetRenderWidgetHost() !=
                        parent->GetRenderWidgetHost();
}

}  // namespace

TabActiveObserver::TabActiveObserver(SkillRecordingService* service,
                                     content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      service_(CHECK_DEREF(service)) {
  CHECK(web_contents);
  web_contents->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        MaybeObserveRenderWidgetHost(render_frame_host);
      });
}

TabActiveObserver::~TabActiveObserver() = default;

void TabActiveObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  MaybeObserveRenderWidgetHost(render_frame_host);
}

void TabActiveObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  MaybeObserveRenderWidgetHost(new_host);
}

void TabActiveObserver::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  std::optional<PendingMouseDown> pending =
      std::exchange(pending_mouse_down_, std::nullopt);
  if (event.GetType() != blink::WebInputEvent::Type::kMouseDown) {
    return;
  }
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();
  if (!main_rfh || !main_rfh->IsActive() || main_rfh->IsErrorDocument() ||
      !web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  const auto& mouse_event = static_cast<const blink::WebMouseEvent&>(event);
  if (mouse_event.button != blink::WebPointerProperties::Button::kLeft) {
    return;
  }

  if (!pending.has_value() || pending.value().timestamp != event.TimeStamp()) {
    return;
  }
  content::RenderFrameHost* target_rfh =
      content::RenderFrameHost::FromFrameToken(pending.value().frame_token);
  ClickTargetResolution resolution = ResolveClickedElementLabel(
      *main_rfh, target_rfh, mouse_event.PositionInWidget());
  std::string description;
  switch (resolution.status) {
    case ClickTargetResolution::Status::kResolved:
      description = base::StrCat({"Clicked on '", resolution.label, "'"});
      break;
    case ClickTargetResolution::Status::kTreeUnavailable:
      if (target_rfh == main_rfh) {
        const int x = base::ClampRound(mouse_event.PositionInWidget().x());
        const int y = base::ClampRound(mouse_event.PositionInWidget().y());
        description = base::StringPrintf("Clicked at X:%d, Y:%d", x, y);
      } else {
        description = "Clicked on page";
      }
      break;
    case ClickTargetResolution::Status::kIgnored:
      return;
  }

  service_->CaptureAction(
      browser::context_hub::mojom::SkillRecordingActionType::kLeftClick,
      std::move(description));
}

void TabActiveObserver::OnInputEvent(const content::RenderWidgetHost& host,
                                     const blink::WebInputEvent& event,
                                     InputEventSource source) {
  if (event.GetType() != blink::WebInputEvent::Type::kMouseDown) {
    return;
  }
  pending_mouse_down_.reset();
  if (static_cast<const blink::WebMouseEvent&>(event).button !=
      blink::WebPointerProperties::Button::kLeft) {
    return;
  }
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();
  if (!main_rfh) {
    return;
  }
  main_rfh->ForEachRenderFrameHostWithAction(
      [this, main_rfh, &host, &event](content::RenderFrameHost* rfh) {
        if (rfh->GetMainFrame() != main_rfh) {
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }
        if (rfh->IsActive() && IsLocalRootFrame(rfh) &&
            rfh->GetRenderWidgetHost() == &host) {
          pending_mouse_down_ = PendingMouseDown{
              .frame_token = rfh->GetGlobalFrameToken(),
              .timestamp = event.TimeStamp(),
          };
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
}

void TabActiveObserver::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  if (input_event_observations_.IsObservingSource(widget_host)) {
    input_event_observations_.RemoveObservation(widget_host);
  }
  if (widget_host_observations_.IsObservingSource(widget_host)) {
    widget_host_observations_.RemoveObservation(widget_host);
  }
}

void TabActiveObserver::MaybeObserveRenderWidgetHost(
    content::RenderFrameHost* render_frame_host) {
  if (!IsLocalRootFrame(render_frame_host) ||
      content::WebContents::FromRenderFrameHost(render_frame_host) !=
          web_contents() ||
      render_frame_host->IsNestedWithinFencedFrame() ||
      render_frame_host->GetOutermostMainFrameOrEmbedder() !=
          render_frame_host->GetMainFrame()) {
    return;
  }
  content::RenderWidgetHost* widget_host =
      render_frame_host->GetRenderWidgetHost();
  if (!widget_host ||
      widget_host_observations_.IsObservingSource(widget_host)) {
    return;
  }
  widget_host_observations_.AddObservation(widget_host);
  input_event_observations_.AddObservation(widget_host);
}

}  // namespace context_hub
