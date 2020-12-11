// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_DURATION_OBSERVER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_DURATION_OBSERVER_H_

#include "base/macros.h"
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
  ~DesktopSessionDurationObserver() override;

  static DesktopSessionDurationObserver* CreateForWebContents(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<DesktopSessionDurationObserver>;

  // Register / Unregister input event callback to given RenderViewHost
  void RegisterInputEventObserver(content::RenderViewHost* host);
  void UnregisterInputEventObserver(content::RenderViewHost* host);

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const blink::WebInputEvent& event) override;

  // content::WebContentsObserver:
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  DesktopSessionDurationTracker* service_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionDurationObserver);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_DESKTOP_SESSION_DURATION_OBSERVER_H_
