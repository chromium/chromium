// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_RENDERER_UPTIME_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_METRICS_RENDERER_UPTIME_WEB_CONTENTS_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace metrics {

// Observer for tracking web contents events.
class RendererUptimeWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RendererUptimeWebContentsObserver> {
 public:
  static RendererUptimeWebContentsObserver* CreateForWebContents(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<RendererUptimeWebContentsObserver>;

  explicit RendererUptimeWebContentsObserver(
      content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DocumentAvailableInMainFrame() override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(RendererUptimeWebContentsObserver);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_RENDERER_UPTIME_WEB_CONTENTS_OBSERVER_H_
