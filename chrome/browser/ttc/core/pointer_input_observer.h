// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_POINTER_INPUT_OBSERVER_H_
#define CHROME_BROWSER_TTC_CORE_POINTER_INPUT_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
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

namespace ttc {

class PointerTracker;

// Feeds a PointerTracker with the mouse's movements over a WebContents.
// TODO(crbug.com/571268337): Record touch too. That needs care with several
// fingers (the moving one, not touches[0]), hovering styluses, and Android's
// InputOnViz, which withholds touch moves from input event observers.
//
// Input reaches a page through the RenderWidgetHost of whichever local root
// frame is under the pointer, so an out-of-process iframe's events never pass
// through the main frame's widget. This observes every widget of the contents,
// following frames as they are created, swapped and destroyed, and converts
// each widget's positions to the root widget's DIP space.
class PointerInputObserver
    : public content::WebContentsObserver,
      public content::RenderWidgetHostObserver,
      public content::RenderWidgetHost::InputEventObserver {
 public:
  // Records the pointer events of `web_contents` into `tracker`. Both must be
  // non-null and outlive this; create a new observer per WebContents.
  PointerInputObserver(PointerTracker* tracker,
                       content::WebContents* web_contents);
  PointerInputObserver(const PointerInputObserver&) = delete;
  PointerInputObserver& operator=(const PointerInputObserver&) = delete;
  ~PointerInputObserver() override;

 private:
  // content::WebContentsObserver:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(content::RenderWidgetHost* host) override;

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const content::RenderWidgetHost& host,
                    const blink::WebInputEvent& event,
                    InputEventSource source) override;

  // Starts observing the widget of `render_frame_host`, if not yet observed.
  void ObserveFrame(content::RenderFrameHost* render_frame_host);

  const raw_ref<PointerTracker> tracker_;
  // The widgets observed, as both an input event observer and a widget
  // observer (for their destruction).
  base::flat_set<raw_ptr<content::RenderWidgetHost>> hosts_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_POINTER_INPUT_OBSERVER_H_
