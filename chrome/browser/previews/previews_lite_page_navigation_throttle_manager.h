// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_MANAGER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_MANAGER_H_

#include <stdint.h>

#include "base/time/time.h"

namespace content {
class WebContents;
}

// This interface specifies the interaction that a
// |PreviewsLitePageNavigationThrottle| has with it's state manager. This class
// tracks the state of the Navigation Throttle since a single instance of the
// navigation throttle can be very short lived, and therefore is not aware of
// any actions taken by its predecessor.
class PreviewsLitePageNavigationThrottleManager {
 public:
  // Used to notify that the Previews Server should not be sent anymore requests
  // until after the given duration.
  virtual void SetServerUnavailableFor(base::TimeDelta retry_after) = 0;

  // Returns true if a Preview should not be triggered because the server is
  // unavailable.
  virtual bool IsServerUnavailable() = 0;

  // Informs the manager that the given URL should be bypassed one time.
  virtual void AddSingleBypass(std::string url) = 0;

  // Queries the manager if the given URL should be bypassed one time, returning
  // true if yes.
  virtual bool CheckSingleBypass(std::string url) = 0;

  // Generates a new page id for a request to the previews server.
  virtual uint64_t GeneratePageID() = 0;

  // Reports data savings to Data Saver.
  virtual void ReportDataSavings(int64_t network_bytes,
                                 int64_t original_bytes,
                                 const std::string& host) = 0;

  // Note: |NeedsToToNotify| is intentionally separate from |NotifyUser| for
  // ease of testing and metrics collection without changing the notification
  // state.
  // Returns true if the UI notification needs to be shown to the user before
  // this preview can be shown.
  virtual bool NeedsToNotifyUser() = 0;

  // Prompts |this| to display the required UI notifications to the user.
  virtual void NotifyUser(content::WebContents* web_contents) = 0;

  // Blacklists the given |host| for the given |duration|.
  virtual void BlacklistHost(const std::string& host,
                             base::TimeDelta duration) = 0;

  // Returns true if the given |host| is blacklisted.
  virtual bool HostBlacklisted(const std::string& host) = 0;
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_MANAGER_H_
