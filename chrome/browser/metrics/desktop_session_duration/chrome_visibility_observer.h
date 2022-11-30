// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_CHROME_VISIBILITY_OBSERVER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_CHROME_VISIBILITY_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace metrics {
// Observer for tracking browser visibility events.
class ChromeVisibilityObserver : public BrowserListObserver {
 public:
  ChromeVisibilityObserver();

  ChromeVisibilityObserver(const ChromeVisibilityObserver&) = delete;
  ChromeVisibilityObserver& operator=(const ChromeVisibilityObserver&) = delete;

  ~ChromeVisibilityObserver() override;

 private:
  friend class ChromeVisibilityObserverInteractiveTest;

  // Notifies |DesktopSessionDurationTracker| of visibility changes. Overridden
  // by tests.
  virtual void SendVisibilityChangeEvent(bool active, base::TimeDelta time_ago);

  // Cancels visibility change in case when the browser becomes visible after a
  // short gap.
  void CancelVisibilityChange();

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // Sets |visibility_gap_timeout_| based on variation params.
  void InitVisibilityGapTimeout();

  void SetVisibilityGapTimeoutForTesting(base::TimeDelta timeout);

  // Timeout interval for waiting after loss of visibility. This allows merging
  // two visibility session if they happened very shortly after each other, for
  // example, when user switching between two browser windows.
  base::TimeDelta visibility_gap_timeout_;

  base::WeakPtrFactory<ChromeVisibilityObserver> weak_factory_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_SESSION_DURATION_CHROME_VISIBILITY_OBSERVER_H_
