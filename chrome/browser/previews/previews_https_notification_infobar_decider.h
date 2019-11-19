// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_HTTPS_NOTIFICATION_INFOBAR_DECIDER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_HTTPS_NOTIFICATION_INFOBAR_DECIDER_H_

#include <stdint.h>

#include "base/time/time.h"

namespace content {
class WebContents;
}

// This specifies an interface for deciding to and displaying the InfoBar that
// tells the user that Data Saver now also optimizes HTTPS pages.
class PreviewsHTTPSNotificationInfoBarDecider {
 public:
  // Note: |NeedsToToNotify| is intentionally separate from |NotifyUser| for
  // ease of testing and metrics collection without changing the notification
  // state.
  // Returns true if the UI notification needs to be shown to the user before
  // this preview can be shown.
  virtual bool NeedsToNotifyUser() = 0;

  // Prompts |this| to display the required UI notifications to the user.
  virtual void NotifyUser(content::WebContents* web_contents) = 0;
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_HTTPS_NOTIFICATION_INFOBAR_DECIDER_H_
