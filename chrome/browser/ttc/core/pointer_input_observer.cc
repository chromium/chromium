// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_input_observer.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace ttc {

PointerInputObserver::PointerInputObserver(PointerTracker* tracker,
                                           content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      tracker_(CHECK_DEREF(tracker)) {
  CHECK(web_contents);
  web_contents->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        ObserveFrame(render_frame_host);
      });
}

PointerInputObserver::~PointerInputObserver() {
  for (content::RenderWidgetHost* host : hosts_) {
    host->RemoveInputEventObserver(this);
    host->RemoveObserver(this);
  }
}

void PointerInputObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // Fires for new frames (with a null `old_host`), cross-process navigations,
  // back/forward cache restores and prerender activations.
  ObserveFrame(new_host);
}

void PointerInputObserver::PrimaryPageChanged(content::Page& page) {
  tracker_->Clear();
}

void PointerInputObserver::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* host) {
  host->RemoveInputEventObserver(this);
  host->RemoveObserver(this);
  hosts_.erase(host);
}

void PointerInputObserver::OnInputEvent(const content::RenderWidgetHost& host,
                                        const blink::WebInputEvent& event,
                                        InputEventSource source) {
  const bool is_press =
      event.GetType() == blink::WebInputEvent::Type::kMouseDown;
  if (!is_press && event.GetType() != blink::WebInputEvent::Type::kMouseMove) {
    return;
  }
  const gfx::PointF position =
      static_cast<const blink::WebMouseEvent&>(event).PositionInWidget();

  // Positions are in the widget's own space; an out-of-process iframe's widget
  // is offset from the root.
  const content::RenderWidgetHostView* view = host.GetView();
  if (!view) {
    return;
  }
  tracker_->RecordSample(event.TimeStamp(),
                         view->TransformPointToRootCoordSpaceF(position),
                         is_press);
}

void PointerInputObserver::ObserveFrame(
    content::RenderFrameHost* render_frame_host) {
  // Frames in the same process as their parent share its widget.
  content::RenderWidgetHost* host = render_frame_host->GetRenderWidgetHost();
  if (host && hosts_.insert(host).second) {
    host->AddInputEventObserver(this);
    host->AddObserver(this);
  }
}

}  // namespace ttc
