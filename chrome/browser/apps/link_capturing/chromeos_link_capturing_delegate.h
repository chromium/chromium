// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_LINK_CAPTURING_DELEGATE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_LINK_CAPTURING_DELEGATE_H_

#include "base/auto_reset.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"

class GURL;
class Profile;

namespace base {
class TickClock;
}

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace apps {

class ChromeOsLinkCapturingDelegate
    : public apps::LinkCapturingNavigationThrottle::Delegate {
 public:
  ChromeOsLinkCapturingDelegate();
  ~ChromeOsLinkCapturingDelegate() override;

  // Method intended for testing purposes only.
  // Set clock used for timing to enable manipulation during tests.
  static base::AutoReset<const base::TickClock*> SetClockForTesting(
      const base::TickClock* tick_clock);

  // apps::LinkCapturingNavigationThrottle::Delegate:
  bool ShouldCancelThrottleCreation(content::NavigationHandle* handle) override;
  absl::optional<apps::LinkCapturingNavigationThrottle::LaunchCallback>
  CreateLinkCaptureLaunchClosure(Profile* profile,
                                 content::WebContents* web_contents,
                                 const GURL& url,
                                 bool is_navigation_from_link) final;

 private:
  base::WeakPtrFactory<ChromeOsLinkCapturingDelegate> weak_factory_{this};
};
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_LINK_CAPTURING_DELEGATE_H_
