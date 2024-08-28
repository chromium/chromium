// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/apps/link_capturing/link_capturing_tab_data.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"  // nogncheck https://crbug.com/1474116
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"  // nogncheck https://crbug.com/1474116
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"  // nogncheck https://crbug.com/1474116
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"  // nogncheck https://crbug.com/1474984
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "url/origin.h"

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

// Retrieves the 'starting' url for the given navigation handle. This considers
// the referrer url, last committed url, and the initiator origin.
GURL GetStartingUrl(content::NavigationHandle* navigation_handle) {
  // This helps us determine a reference GURL for the current NavigationHandle.
  // This is the order or preference: Referrer > LastCommittedURL >
  // InitiatorOrigin. InitiatorOrigin *should* only be used on very rare cases,
  // e.g. when the navigation goes from https: to http: on a new tab, thus
  // losing the other potential referrers.
  const GURL referrer_url = navigation_handle->GetReferrer().url;
  if (referrer_url.is_valid() && !referrer_url.is_empty()) {
    return referrer_url;
  }

  const GURL last_committed_url =
      navigation_handle->GetWebContents()->GetLastCommittedURL();
  if (last_committed_url.is_valid() && !last_committed_url.is_empty()) {
    return last_committed_url;
  }

  const auto& initiator_origin = navigation_handle->GetInitiatorOrigin();
  return initiator_origin.has_value() ? initiator_origin->GetURL() : GURL();
}

// Returns if the navigation appears to be a link navigation, but not from an
// HTML post form.
bool IsNavigateFromNonFormNonContextMenuLink(
    content::NavigationHandle* navigation_handle) {
  // Always handle http(s) <form> submissions in Chrome for two reasons: 1) we
  // don't have a way to send POST data to ARC, and 2) intercepting http(s) form
  // submissions is not very important because such submissions are usually
  // done within the same domain. ShouldOverrideUrlLoading() below filters out
  // such submissions anyway.
  constexpr bool kAllowFormSubmit = false;

  ui::PageTransition page_transition = navigation_handle->GetPageTransition();

  return LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
             page_transition, kAllowFormSubmit,
             navigation_handle->IsInFencedFrameTree(),
             navigation_handle->HasUserGesture()) &&
         !navigation_handle->WasStartedFromContextMenu();
}

bool IsNavigationUserInitiated(content::NavigationHandle* handle) {
  switch (handle->GetNavigationInitiatorActivationAndAdStatus()) {
    case blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kDidNotStartWithTransientActivation:
      return false;
    case blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kStartedWithTransientActivationFromNonAd:
    case blink::mojom::NavigationInitiatorActivationAndAdStatus::
        kStartedWithTransientActivationFromAd:
      return true;
  }
}

}  // namespace

bool LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
    ui::PageTransition page_transition,
    bool allow_form_submit,
    bool is_in_fenced_frame_tree,
    bool has_user_gesture) {
  // Navigations inside fenced frame trees are marked with
  // PAGE_TRANSITION_AUTO_SUBFRAME in order not to add session history items
  // (see https://crrev.com/c/3265344). So we only check |has_user_gesture|.
  if (is_in_fenced_frame_tree) {
    DCHECK(ui::PageTransitionCoreTypeIs(page_transition,
                                        ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    return has_user_gesture;
  }

  // Mask out any redirect qualifiers
  page_transition = MaskOutPageTransition(page_transition,
                                          ui::PAGE_TRANSITION_IS_REDIRECT_MASK);

  if (!ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_LINK) &&
      !(allow_form_submit &&
        ui::PageTransitionCoreTypeIs(page_transition,
                                     ui::PAGE_TRANSITION_FORM_SUBMIT))) {
    // Do not handle the |url| if this event wasn't spawned by the user clicking
    // on a link.
    return false;
  }

  if (base::to_underlying(ui::PageTransitionGetQualifier(page_transition)) !=
      0) {
    // Qualifiers indicate that this navigation was the result of a click on a
    // forward/back button, or typing in the URL bar. Don't handle any of those
    // types of navigations.
    return false;
  }

  return true;
}

ui::PageTransition LinkCapturingNavigationThrottle::MaskOutPageTransition(
    ui::PageTransition page_transition,
    ui::PageTransition mask) {
  return ui::PageTransitionFromInt(page_transition & ~mask);
}

// static
bool LinkCapturingNavigationThrottle::
    IsEmptyDanglingWebContentsAfterLinkCapture(
        content::NavigationHandle* handle) {
  const GURL& last_committed_url =
      handle->GetWebContents()->GetLastCommittedURL();
  return !last_committed_url.is_valid() || last_committed_url.IsAboutBlank() ||
         // Some navigations are via JavaScript `location.href = url;`.
         // This can be used for user clicked buttons as well as redirects.
         // Check whether the action was in the context of a user activation to
         // distinguish redirects from click event handlers.
         !IsNavigationUserInitiated(handle);
}

LinkCapturingNavigationThrottle::Delegate::~Delegate() = default;

// static
std::unique_ptr<content::NavigationThrottle>
LinkCapturingNavigationThrottle::MaybeCreate(
    content::NavigationHandle* handle,
    std::unique_ptr<Delegate> delegate) {
  // If the reimplementation params of the link capturing feature flag is
  // enabled, turn off the "old" link capturing behavior.
  if (features::IsNavigationCapturingReimplEnabled()) {
    return nullptr;
  }

  // Don't handle navigations in subframes or main frames that are in a nested
  // frame tree (e.g. fenced-frame). We specifically allow
  // prerendering navigations so that we can destroy the prerender. Opening an
  // app must only happen when the user intentionally navigates; however, for a
  // prerender, the prerender-activating navigation doesn't run throttles so we
  // must cancel it during initial loading to get a standard (non-prerendering)
  // navigation at link-click-time.
  if (!handle->IsInOutermostMainFrame()) {
    return nullptr;
  }

  content::WebContents* web_contents = handle->GetWebContents();
  if (prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents) != nullptr) {
    return nullptr;
  }

  if (delegate->ShouldCancelThrottleCreation(handle)) {
    return nullptr;
  }

  // If there is no browser attached to this web-contents yet, this was a
  // middle-mouse-click action, which should not be captured.
  // TODO(crbug.com/40279479): Find a better way to detect middle-clicks.
  if (chrome::FindBrowserWithTab(web_contents) == nullptr) {
    return nullptr;
  }

  // Never link capture links that open in a popup window. Popups are closely
  // associated with the tab that opened them, so the popup should open in the
  // same (app/non-app) context as its opener.
  WindowOpenDisposition disposition =
      GetLinkCapturingSourceDisposition(web_contents);
  if (disposition == WindowOpenDisposition::NEW_POPUP &&
      !web_contents->GetLastCommittedURL().is_valid()) {
    return nullptr;
  }

  return base::WrapUnique(
      new LinkCapturingNavigationThrottle(handle, std::move(delegate)));
}

LinkCapturingNavigationThrottle::LaunchCallbackForTesting&
LinkCapturingNavigationThrottle::GetLinkCaptureLaunchCallbackForTesting() {
  static base::NoDestructor<LaunchCallbackForTesting> callback;
  return *callback;
}

LinkCapturingNavigationThrottle::~LinkCapturingNavigationThrottle() = default;

const char* LinkCapturingNavigationThrottle::GetNameForLogging() {
  return "LinkCapturingNavigationThrottle";
}

ThrottleCheckResult LinkCapturingNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  starting_url_ = GetStartingUrl(navigation_handle());
  return HandleRequest();
}

ThrottleCheckResult LinkCapturingNavigationThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return HandleRequest();
}

// Returns true if |url| is a known and valid redirector that will redirect a
// navigation elsewhere.
// static
bool LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(const GURL& url) {
  // This currently only check for redirectors on the "google" domain.
  if (!page_load_metrics::IsGoogleSearchHostname(url)) {
    return false;
  }

  return url.path_piece() == "/url" && url.has_query();
}

// If the previous url and current url are not the same (AKA a redirection),
// determines if the redirection should be considered for an app launch. Returns
// false for redirections where:
// * `previous_url` is an extension.
// * `previous_url` is a google redirector.
// static
bool LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
    const GURL& previous_url,
    const GURL& current_url) {
  // Check the scheme for both |previous_url| and |current_url| since an
  // extension could have referred us (e.g. Google Docs).
  if (previous_url.SchemeIs(extensions::kExtensionScheme)) {
    return false;
  }

  // Skip URL redirectors that are intermediate pages redirecting towards a
  // final URL.
  if (IsGoogleRedirectorUrl(current_url)) {
    return false;
  }

  return true;
}

ThrottleCheckResult LinkCapturingNavigationThrottle::HandleRequest() {
  content::NavigationHandle* handle = navigation_handle();

  // If the navigation will update the same document, don't consider as a
  // capturable link.
  if (handle->IsSameDocument()) {
    return content::NavigationThrottle::PROCEED;
  }

  const GURL& url = handle->GetURL();
  if (!url.is_valid()) {
    DVLOG(1) << "Unexpected URL: " << url << ", opening in Chrome.";
    return content::NavigationThrottle::PROCEED;
  }

  // Only http-style schemes are allowed.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return content::NavigationThrottle::PROCEED;
  }

  if (!ShouldOverrideUrlIfRedirected(starting_url_, url)) {
    return content::NavigationThrottle::PROCEED;
  }

  bool is_navigation_from_link =
      IsNavigateFromNonFormNonContextMenuLink(handle);

  content::WebContents* web_contents = handle->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::optional<LaunchCallback> launch_link_capture =
      delegate_->CreateLinkCaptureLaunchClosure(profile, web_contents, url,
                                                is_navigation_from_link);
  if (!launch_link_capture.has_value()) {
    return content::NavigationThrottle::PROCEED;
  }

  // If this is a prerender navigation that would otherwise launch an app, we
  // must cancel it. We only want to launch an app once the URL is intentionally
  // navigated to by the user. We cancel the navigation here so that when the
  // link is clicked, we'll run NavigationThrottles again. If we leave the
  // prerendering alive, the activating navigation won't run throttles.
  if (handle->IsInPrerenderedMainFrame()) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  // Browser & profile keep-alives must be used to keep the browser & profile
  // alive because the old window is required to be closed before the new app is
  // launched, which will destroy the profile & browser if it is the last
  // window.
  // Why close the tab first? The way web contents currently work, closing a tab
  // in a window will re-activate that window if there are more tabs there. So
  // if we wait until after the launch completes to close the tab, then it will
  // cause the old window to come to the front hiding the newly launched app
  // window.
  bool closed_web_contents = false;
  std::unique_ptr<ScopedKeepAlive> browser_keep_alive;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
  if (IsEmptyDanglingWebContentsAfterLinkCapture(handle)) {
    browser_keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::APP_LAUNCH, KeepAliveRestartOption::ENABLED);
    if (!profile->IsOffTheRecord()) {
      profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
          profile, ProfileKeepAliveOrigin::kAppWindow);
    }
    web_contents->ClosePage();
    closed_web_contents = true;
  }

  // Note: This callback currently serves to own the "keep alive" objects
  // until the launch is complete.
  base::OnceClosure launch_callback = base::BindOnce(
      [](std::unique_ptr<ScopedKeepAlive> browser_keep_alive,
         std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
         bool closed_web_contents) {
        if (GetLinkCaptureLaunchCallbackForTesting()) {        // IN-TEST
          std::move(GetLinkCaptureLaunchCallbackForTesting())  // IN-TEST
              .Run(closed_web_contents);                       // IN-TEST
        }
      },
      std::move(browser_keep_alive), std::move(profile_keep_alive),
      closed_web_contents);

  // The tab may have been closed, which runs async and causes the browser
  // window to be refocused. Post a task to launch the app to ensure launching
  // happens after the tab closed, otherwise the opened app window might be
  // inactivated.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(launch_link_capture.value()),
                                std::move(launch_callback)));
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

LinkCapturingNavigationThrottle::LinkCapturingNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<Delegate> delegate)
    : content::NavigationThrottle(navigation_handle),
      delegate_(std::move(delegate)) {}

}  // namespace apps
