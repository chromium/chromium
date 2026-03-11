// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SCROLL_OBSERVER_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SCROLL_OBSERVER_H_

#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace send_tab_to_self {

// Observes scroll interactions on a WebContents that was opened via
// Send Tab to Self. Measured for metrics purposes only.
class SendTabToSelfScrollObserver
    : public content::WebContentsObserver,
      public content::RenderWidgetHost::InputEventObserver,
      public content::RenderWidgetHostObserver,
      public content::WebContentsUserData<SendTabToSelfScrollObserver> {
 public:
  SendTabToSelfScrollObserver(const SendTabToSelfScrollObserver&) = delete;
  SendTabToSelfScrollObserver& operator=(const SendTabToSelfScrollObserver&) =
      delete;

  ~SendTabToSelfScrollObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const content::RenderWidgetHost& host,
                    const blink::WebInputEvent& event,
                    input::InputEventSource source) override;

 private:
  friend class content::WebContentsUserData<SendTabToSelfScrollObserver>;

  SendTabToSelfScrollObserver(content::WebContents* web_contents,
                              bool restoration_attempted);

  void RecordMetrics();

  float total_scroll_volume_ = 0.0f;
  raw_ptr<content::RenderWidgetHost> observed_host_ = nullptr;
  const bool restoration_attempted_;
  bool metrics_recorded_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SCROLL_OBSERVER_H_
