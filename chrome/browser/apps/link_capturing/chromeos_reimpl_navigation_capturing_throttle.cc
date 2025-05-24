// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/chromeos_reimpl_navigation_capturing_throttle.h"

#include <memory>
#include <string>

#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/values_equivalent.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"
#include "chrome/browser/apps/link_capturing/link_capturing_tab_data.h"
#include "chrome/browser/apps/link_capturing/metrics/intent_handling_metrics.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"  // nogncheck https://crbug.com/1474116
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"  // nogncheck https://crbug.com/1474116
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"  // nogncheck https://crbug.com/1474116
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"  // nogncheck https://crbug.com/1474984
#include "chrome/browser/ui/web_applications/navigation_capturing_process.h"  // nogncheck https://crbug.com/377760841
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "chrome/browser/web_applications/navigation_capturing_log.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/url_constants.h"

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

bool IsSystemWebApp(Profile* profile, const std::string& app_id) {
  bool is_system_web_app = false;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&is_system_web_app](const apps::AppUpdate& update) {
        if (update.InstallReason() == apps::InstallReason::kSystem) {
          is_system_web_app = true;
        }
      });
  return is_system_web_app;
}

// Used to create a unique timestamped URL to force reload apps.
// Points to the base::DefaultTickClock by default.
static const base::TickClock*& GetTickClock() {
  static const base::TickClock* g_clock = base::DefaultTickClock::GetInstance();
  return g_clock;
}

// This function redirects an external untrusted |url| to a privileged trusted
// one for SWAs, if applicable.
GURL RedirectUrlIfProjectorApp(Profile* profile,
                               const std::string& app_id,
                               const GURL& url) {
  if (!IsSystemWebApp(profile, app_id)) {
    return url;
  }

  bool is_projector_app =
      app_id == ash::kChromeUIUntrustedProjectorSwaAppId &&
      url.GetWithEmptyPath() == GURL(ash::kChromeUIUntrustedProjectorPwaUrl);
  if (!is_projector_app) {
    return url;
  }

  // Handle projector app redirection.
  std::string override_url = ash::kChromeUIUntrustedProjectorUrl;
  if (url.path().length() > 1) {
    override_url += url.path().substr(1);
  }
  std::stringstream ss;
  // Since ChromeOS doesn't reload an app if the URL doesn't change, the line
  // below appends a unique timestamp to the URL to force a reload.
  // TODO(b/211787536): Remove the timestamp after we update the trusted URL
  // to match the user's navigations through the post message api.
  ss << override_url << "?timestamp=" << GetTickClock()->NowTicks();

  if (url.has_query()) {
    ss << '&' << url.query();
  }

  GURL result(ss.str());
  CHECK(result.is_valid());
  return result;
}

IntentHandlingMetrics::Platform GetMetricsPlatform(AppType app_type) {
  switch (app_type) {
    case AppType::kArc:
      return IntentHandlingMetrics::Platform::ARC;
    case AppType::kWeb:
    case AppType::kSystemWeb:
      return IntentHandlingMetrics::Platform::PWA;
    case AppType::kUnknown:
    case AppType::kCrostini:
    case AppType::kChromeApp:
    case AppType::kPluginVm:
    case AppType::kRemote:
    case AppType::kBorealis:
    case AppType::kExtension:
    case AppType::kBruschetta:
      NOTREACHED();
  }
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

void LaunchApp(base::WeakPtr<AppServiceProxy> proxy,
               const std::string& app_id,
               int32_t event_flags,
               GURL url,
               LaunchSource launch_source,
               WindowInfoPtr window_info,
               AppType app_type,
               base::OnceClosure callback) {
  if (!proxy) {
    return;
  }

  proxy->LaunchAppWithUrl(
      app_id, event_flags, url, launch_source, std::move(window_info),
      base::IgnoreArgs<LaunchResult&&>(std::move(callback)));

  IntentHandlingMetrics::RecordPreferredAppLinkClickMetrics(
      GetMetricsPlatform(app_type));
}

ui::PageTransition MaskOutPageTransition(ui::PageTransition page_transition,
                                         ui::PageTransition mask) {
  return ui::PageTransitionFromInt(page_transition & ~mask);
}

bool IsCapturableLinkNavigation(ui::PageTransition page_transition,
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

  // Note: This means that <form> submissions are not handled. They should
  // always be handled by http(s) <form> submissions in Chrome for two reasons:
  // 1) we don't have a way to send POST data to ARC, and
  // 2) intercepting http(s) form submissions is not very important because such
  //    submissions are usually done within the same domain.
  if (!ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_LINK)) {
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

// Returns true if |url| is a known and valid redirector that will redirect a
// navigation elsewhere.
// static
bool IsGoogleRedirectorUrl(const GURL& url) {
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
bool ShouldCaptureUrlIfRedirected(const GURL& previous_url,
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

// Retrieves the 'starting' url for the given navigation handle. This considers
// the referrer url, last committed url, and the initiator origin.
GURL GetStartingUrl(content::NavigationHandle* navigation_handle) {
  // This helps us determine a reference GURL for the current NavigationHandle.
  // This is the order or preference: Referrer > LastCommittedURL >
  // InitiatorOrigin. InitiatorOrigin *should* only be used on very rare cases,
  // e.g. when the navigation goes from https: to http: on a new tab, thus
  // losing the other potential referrers.
  const GURL referrer_url = navigation_handle->GetReferrer().url;
  if (referrer_url.is_valid()) {
    return referrer_url;
  }

  const GURL last_committed_url =
      navigation_handle->GetWebContents()->GetLastCommittedURL();
  if (last_committed_url.is_valid()) {
    return last_committed_url;
  }

  const auto& initiator_origin = navigation_handle->GetInitiatorOrigin();
  return initiator_origin.has_value() ? initiator_origin->GetURL() : GURL();
}

// Returns if the navigation appears to be a link navigation, but not from an
// HTML post form.
bool IsNavigateFromNonFormNonContextMenuLink(
    content::NavigationHandle* navigation_handle) {
  ui::PageTransition page_transition = navigation_handle->GetPageTransition();

  return IsCapturableLinkNavigation(page_transition,
                                    navigation_handle->IsInFencedFrameTree(),
                                    navigation_handle->HasUserGesture()) &&
         !navigation_handle->WasStartedFromContextMenu();
}

// Returns if the the link navigation should be handled, either for web apps or
// arc apps. If this is a prerender navigation, then returning 'true' here will
// cancel the prerender in preparation of having the throttle run again when
// `NavigationCapturingProcess` is attached to the `NavigationHandle`.
bool ShouldThrottleCaptureNavigation(
    AppType app_type,
    const GURL& starting_url,
    const AppIdsToLaunchForUrl& app_ids_to_launch,
    bool is_link_click,
    bool is_for_projector_swa,
    content::NavigationHandle* handle,
    base::Value::Dict* debug_dict) {
  content::WebContents* web_contents = handle->GetWebContents();
  CHECK(web_contents);
  CHECK(app_ids_to_launch.preferred);

  webapps::AppId launch_app_id = *app_ids_to_launch.preferred;

  bool is_for_cros_experiment_app =
      web_app::ChromeOsWebAppExperiments::ShouldLaunchForRedirectedNavigation(
          launch_app_id);
  debug_dict->Set("is_for_cros_experiment_app", is_for_cros_experiment_app);
  if (app_type == AppType::kWeb) {
    if (!base::FeatureList::IsEnabled(
            features::kNavigationCapturingOnExistingFrames) &&
        !is_for_cros_experiment_app && !is_for_projector_swa) {
      debug_dict->Set("!result", "existing frame disabled");
      return false;
    }
    bool is_new_frame_capture_for_non_prerendering =
        web_app::NavigationCapturingProcess::GetForNavigationHandle(*handle);
    debug_dict->Set("is_new_frame_capture",
                    is_new_frame_capture_for_non_prerendering);
    if (is_new_frame_capture_for_non_prerendering) {
      return false;
    }
    // Note: During prerendering, for navigations that would normally be cause
    // by the NavigationCapturingProcess, they go through here still. This is an
    // unfortunate edge case as it's not possible to know if the prerender is
    // for a new frame or an existing one. The responsibility of this throttle
    // is to handle the 'existing frame' cases, and to function all prerenders
    // for that case must be cancelled. So - this means that ALL prerenders are
    // then cancelled.
    // TODO(https://crbug.com/397964745): Have a better way to allow
    // prerendering to occur and still allow this supplemental capturing.
  }

  // Non-link-clicks are OK if we are going to the ChromeOS experiment app,
  // which only needs this behavior from an "about:blank" page.
  debug_dict->Set("redirect_chain_size",
                  static_cast<int>(handle->GetRedirectChain().size()));
  if (!is_for_projector_swa && !is_link_click &&
      !(handle->GetRedirectChain().size() > 1 && is_for_cros_experiment_app &&
        starting_url == GURL(url::kAboutBlankURL))) {
    return false;
  }

  // Don't capture if already inside the target app scope.
  // TODO(crbug.com/313518305): Query App Service intent filters instead, so
  // that this check also covers ARC apps.
  web_app::WebAppTabHelper* tab_helper =
      web_app::WebAppTabHelper::FromWebContents(web_contents);
  if (app_type == AppType::kWeb && tab_helper &&
      tab_helper->app_id() == launch_app_id) {
    debug_dict->Set("!result", "already in scope");
    return false;
  }

  // Don't capture if already inside a Web App window for the target app. If the
  // previous early return didn't trigger, this means we are in an app window
  // but out of scope of the original app, and navigating will put us back in
  // scope.
  if (tab_helper && tab_helper->window_app_id() == launch_app_id) {
    debug_dict->Set("!result", "already in window");
    return false;
  }

  return true;
}

}  // namespace

// static
bool ChromeOsReimplNavigationCapturingThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (!features::IsNavigationCapturingReimplEnabled()) {
    return false;
  }

  auto& handle = registry.GetNavigationHandle();
  content::WebContents* contents = handle.GetWebContents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }

  // Don't handle navigations in subframes or main frames that are in a nested
  // frame tree (e.g. fenced-frame).
  if (!handle.IsInOutermostMainFrame()) {
    return false;
  }

  if (prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          contents) != nullptr) {
    return false;
  }

  if (handle.ExistingDocumentWasDiscarded()) {
    return false;
  }

  // If there is no browser attached to this web-contents yet, this was a
  // middle-mouse-click action, which should not be captured.
  // TODO(crbug.com/40279479): Find a better way to detect middle-clicks.
  if (chrome::FindBrowserWithTab(contents) == nullptr) {
    return false;
  }

  // Never link capture links that open in a popup window. Popups are closely
  // associated with the tab that opened them, so the popup should open in the
  // same (app/non-app) context as its opener.
  WindowOpenDisposition disposition =
      GetLinkCapturingSourceDisposition(contents);
  if (disposition == WindowOpenDisposition::NEW_POPUP &&
      !contents->GetLastCommittedURL().is_valid()) {
    return false;
  }

  // Note: We specifically allow prerendering navigations so that we can destroy
  // the prerender. Opening an app must only happen when the user intentionally
  // navigates; however, for a prerender, the prerender-activating navigation
  // doesn't run throttles so we must cancel it during initial loading to get a
  // standard (non-prerendering) navigation at link-click-time.

  registry.AddThrottle(base::WrapUnique(
      new ChromeOsReimplNavigationCapturingThrottle(registry, profile)));
  return true;
}

ChromeOsReimplNavigationCapturingThrottle::
    ~ChromeOsReimplNavigationCapturingThrottle() {
  if (debug_data_.empty()) {
    return;
  }
  web_app::WebAppProvider::GetForWebApps(&profile_.get())
      ->navigation_capturing_log()
      .LogData(GetNameForLogging(), base::Value(std::move(debug_data_)),
               navigation_handle() ? navigation_handle()->GetNavigationId()
                                   : std::optional<int64_t>(std::nullopt));
}

const char* ChromeOsReimplNavigationCapturingThrottle::GetNameForLogging() {
  return "ChromeOsReimplNavigationCapturingThrottle";
}

// static
base::AutoReset<const base::TickClock*>
ChromeOsReimplNavigationCapturingThrottle::SetClockForTesting(
    const base::TickClock* tick_clock) {
  CHECK_IS_TEST();
  return base::AutoReset<const base::TickClock*>(&GetTickClock(), tick_clock);
}

ThrottleCheckResult
ChromeOsReimplNavigationCapturingThrottle::WillStartRequest() {
  return HandleRequest();
}

ThrottleCheckResult
ChromeOsReimplNavigationCapturingThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return HandleRequest();
}

ChromeOsReimplNavigationCapturingThrottle::
    ChromeOsReimplNavigationCapturingThrottle(
        content::NavigationThrottleRegistry& registry,
        Profile* profile)
    : content::NavigationThrottle(registry), profile_(*profile) {}

ThrottleCheckResult ChromeOsReimplNavigationCapturingThrottle::HandleRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(&profile_.get());
  if (!proxy) {
    return content::NavigationThrottle::PROCEED;
  }

  CHECK(navigation_handle());
  content::NavigationHandle* handle = navigation_handle();

  // Exclude #ref navigations, pushState/replaceState, etc. This doesn't exclude
  // target="_self".
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

  GURL starting_url = GetStartingUrl(handle);
  if (!ShouldCaptureUrlIfRedirected(starting_url, url)) {
    return content::NavigationThrottle::PROCEED;
  }

  AppIdsToLaunchForUrl app_ids_to_launch =
      FindAppIdsToLaunchForUrl(proxy, handle->GetURL());

  const std::vector<std::string>& app_candidates = app_ids_to_launch.candidates;
  // If there are no candidates for launching the url in an app or the app
  // is not preferred for launching the url, allow navigation to proceed
  // normally.
  if (app_candidates.empty() || !app_ids_to_launch.preferred.has_value()) {
    return content::NavigationThrottle::PROCEED;
  }

  bool is_for_prerender = handle->IsInPrerenderedMainFrame();
  base::Value::Dict* debug_data = &debug_data_;
  if (is_for_prerender) {
    debug_data = debug_data_.EnsureDict("prerender");
  }
  debug_data->Set("!final_url", url.possibly_invalid_spec());
  debug_data->Set("!starting_url", starting_url.possibly_invalid_spec());
  debug_data->Set("!preferred", *app_ids_to_launch.preferred);
  for (const std::string& candidate : app_candidates) {
    debug_data->EnsureList("app_candidate")->Append(candidate);
  }
  for (const GURL& redirect_url : handle->GetRedirectChain()) {
    debug_data->EnsureList("redirect_chain")
        ->Append(redirect_url.possibly_invalid_spec());
  }
  ui::PageTransition page_transition = navigation_handle()->GetPageTransition();
  debug_data->Set("page_transition_core_transition",
                  ui::PageTransitionGetCoreTransitionString(page_transition));
  ui::PageTransition qualifiers =
      ui::PageTransitionGetQualifier(page_transition);
  debug_data->Set("page_transition_qualifiers",
                  base::HexEncode(base::byte_span_from_ref(qualifiers)));

  const std::string launch_app_id = *app_ids_to_launch.preferred;
  const AppType app_type = proxy->AppRegistryCache().GetAppType(launch_app_id);

  // Only automatically launch supported app types.
  bool system_web_app = IsSystemWebApp(&profile_.get(), launch_app_id);
  debug_data->Set("system_web_app", system_web_app);
  if (app_type != AppType::kArc && app_type != AppType::kWeb &&
      !system_web_app) {
    return content::NavigationThrottle::PROCEED;
  }

  const bool is_for_projector_swa =
      base::Contains(app_candidates, ash::kChromeUIUntrustedProjectorSwaAppId);

  // Note: This is an unfortunate way to detect a link click. If there is a
  // better way to know all of navigation's original disposition, frame, etc,
  // that would be much better.
  bool is_link_click = IsNavigateFromNonFormNonContextMenuLink(handle);
  bool is_capturable = ShouldThrottleCaptureNavigation(
      app_type, starting_url, app_ids_to_launch, is_link_click,
      is_for_projector_swa, handle, debug_data);
  debug_data->Set("is_for_projector_swa", is_for_projector_swa);
  debug_data->Set("is_link_click", is_link_click);
  debug_data->Set("is_capturable", is_capturable);
  if (!is_capturable) {
    debug_data->Set("!result", "not capturable");
    return content::NavigationThrottle::PROCEED;
  }

  // If this is a prerender navigation that would otherwise launch an app, we
  // must cancel it. We only want to launch an app once the URL is intentionally
  // navigated to by the user. We cancel the navigation here so that when the
  // link is clicked, we'll run NavigationThrottles again. If we leave the
  // prerendering alive, the activating navigation won't run throttles.
  // TODO(https://crbug.com/397964745): Have a better way to allow prerendering
  // here, as this stops prerendering for ALL in-app-scope links, whether or not
  // they are captured via the NavigationCapturingProcess or not (as prerender
  // runs without calling browser_navigator.cc, so this throttle is created &
  // used).
  if (is_for_prerender) {
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  auto launch_source = IsNavigateFromNonFormNonContextMenuLink(handle)
                           ? LaunchSource::kFromLink
                           : LaunchSource::kFromOmnibox;
  debug_data->Set("launch_source", base::ToString(launch_source));
  GURL redirected_url =
      is_for_projector_swa
          ? RedirectUrlIfProjectorApp(&profile_.get(), launch_app_id,
                                      handle->GetURL())
          : handle->GetURL();
  debug_data->Set("redirected_url", redirected_url.possibly_invalid_spec());

  // Close existing web contents if it is around.
  std::unique_ptr<ScopedKeepAlive> browser_keep_alive;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
  bool closed_web_contents = false;
  if (IsEmptyDanglingWebContentsAfterLinkCapture()) {
    browser_keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::APP_LAUNCH, KeepAliveRestartOption::ENABLED);
    if (!profile_->IsOffTheRecord()) {
      profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
          &profile_.get(), ProfileKeepAliveOrigin::kAppWindow);
    }
    handle->GetWebContents()->ClosePage();
    closed_web_contents = true;
  }
  debug_data->Set("closed_web_contents", closed_web_contents);
  base::OnceClosure launch_callback = base::BindOnce(
      [](std::unique_ptr<ScopedKeepAlive> browser_keep_alive,
         std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
         bool closed_web_contents) {
        // TODO(https://crbug.com/400473923): Move this to this class when we
        // remove v1, as we'll still keep the tests that use this.
        if (LinkCapturingNavigationThrottle::
                GetLinkCaptureLaunchCallbackForTesting()) {  // IN-TEST
          std::move(LinkCapturingNavigationThrottle::
                        GetLinkCaptureLaunchCallbackForTesting())  // IN-TEST
              .Run(closed_web_contents);
        }
      },
      std::move(browser_keep_alive), std::move(profile_keep_alive),
      closed_web_contents);

  debug_data->Set("!result", "launched");
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&LaunchApp, proxy->GetWeakPtr(), launch_app_id,
                     GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                   /*prefer_container=*/true),
                     redirected_url, launch_source,
                     std::make_unique<WindowInfo>(display::kDefaultDisplayId),
                     app_type, std::move(launch_callback)));

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

bool ChromeOsReimplNavigationCapturingThrottle::
    IsEmptyDanglingWebContentsAfterLinkCapture() {
  CHECK(navigation_handle());
  content::WebContents* contents = navigation_handle()->GetWebContents();
  CHECK(contents);

  const GURL& last_committed_url = contents->GetLastCommittedURL();
  return !last_committed_url.is_valid() || last_committed_url.IsAboutBlank() ||
         // Some navigations are via JavaScript `location.href = url;`.
         // This can be used for user clicked buttons as well as redirects.
         // Check whether the action was in the context of a user activation to
         // distinguish redirects from click event handlers.
         !IsNavigationUserInitiated(navigation_handle());
}

}  // namespace apps
