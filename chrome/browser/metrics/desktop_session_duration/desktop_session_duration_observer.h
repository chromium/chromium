// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_DURATION_OBSERVER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_DURATION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace metrics {

class DesktopSessionDurationTracker;

// Tracks user input events from web contents and notifies
// |DesktopSessionDurationTracker|.
class DesktopSessionDurationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DesktopSessionDurationObserver>,
      public content::RenderWidgetHost::InputEventObserver {
 public:
  DesktopSessionDurationObserver(content::WebContents* web_contents,
                                 DesktopSessionDurationTracker* service);

  DesktopSessionDurationObserver(const DesktopSessionDurationObserver&) =
      delete;
  DesktopSessionDurationObserver& operator=(
      const DesktopSessionDurationObserver&) = delete;

  ~DesktopSessionDurationObserver() override;

  static DesktopSessionDurationObserver* CreateForWebContents(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<DesktopSessionDurationObserver>;

  // Register / Unregister input event callback to given RenderFrameHost
  void RegisterInputEventObserver(content::RenderFrameHost* host);
  void UnregisterInputEventObserver(content::RenderFrameHost* host);

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const blink::WebInputEvent& event) override;

  // content::WebContentsObserver:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  raw_ptr<DesktopSessionDurationTracker> service_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_DURATION_OBSERVER_H_
