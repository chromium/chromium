// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_NAVIGATION_THROTTLE_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "content/public/browser/navigation_throttle.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class Profile;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace apps {

// Allows canceling a navigation to instead be routed to an installed app.
class LinkCapturingNavigationThrottle : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  static bool IsCapturableLinkNavigation(ui::PageTransition page_transition,
                                         bool allow_form_submit,
                                         bool is_in_fenced_frame_tree,
                                         bool has_user_gesture);

  // Removes |mask| bits from |page_transition|.
  static ui::PageTransition MaskOutPageTransition(
      ui::PageTransition page_transition,
      ui::PageTransition mask);

  // Inspects the WebContents of the navigation to determine whether, after
  // successful link capturing, it is a redundant dangling empty browser tab
  // that should be cleaned up. These dangling tabs result from e.g.
  // window.open(url) or target="_blank" navigations.
  static bool IsEmptyDanglingWebContentsAfterLinkCapture(
      content::NavigationHandle* handle);

  using LaunchCallback =
      base::OnceCallback<void(base::OnceClosure on_launch_complete)>;

  class Delegate {
   public:
    virtual ~Delegate();

    virtual bool ShouldCancelThrottleCreation(
        content::NavigationHandle* handle) = 0;

    // If the return value is a nullopt, then no capture was possible.
    // Otherwise, the returned closure will launch the application at the
    // appropriate URL.
    virtual std::optional<LaunchCallback> CreateLinkCaptureLaunchClosure(
        Profile* profile,
        content::WebContents* web_contents,
        const GURL& url,
        bool is_navigation_from_link) = 0;
  };

  // Possibly creates a navigation throttle that checks if any installed apps
  // can handle the URL being navigated to.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle,
      std::unique_ptr<Delegate> delegate);

  using LaunchCallbackForTesting =
      base::OnceCallback<void(bool closed_web_contents)>;
  static LaunchCallbackForTesting& GetLinkCaptureLaunchCallbackForTesting();

  LinkCapturingNavigationThrottle(const LinkCapturingNavigationThrottle&) =
      delete;
  LinkCapturingNavigationThrottle& operator=(
      const LinkCapturingNavigationThrottle&) = delete;
  ~LinkCapturingNavigationThrottle() override;

  // content::NavigationHandle overrides
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

  // Visible for testing.
  static bool IsGoogleRedirectorUrl(const GURL& url);
  // Visible for testing.
  static bool ShouldOverrideUrlIfRedirected(const GURL& previous_url,
                                            const GURL& current_url);

 private:
  explicit LinkCapturingNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<Delegate> delegate);
  std::unique_ptr<Delegate> delegate_;
  GURL starting_url_;

  ThrottleCheckResult HandleRequest();
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_NAVIGATION_THROTTLE_H_
