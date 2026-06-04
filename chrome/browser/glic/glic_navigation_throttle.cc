// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_navigation_throttle.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#endif
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace glic {

// Retrieves the GURL for the Web Continuity URL, defaulting to the Guest URL
// with "/continue" appended.
GURL GetGlicWebContinuityUrl() {
  static const base::NoDestructor<GURL> continuity_url([]() {
    std::string continuity_url_str = features::kGlicWebContinuityUrl.Get();
    if (!continuity_url_str.empty()) {
      GURL url(continuity_url_str);
      if (url.is_valid()) {
        return url;
      }
    }

    std::string guest_url_str = features::kGlicGuestURL.Get();
    if (!guest_url_str.empty()) {
      GURL url(guest_url_str);
      if (url.is_valid()) {
        return GURL(url.spec() + "/continue");
      }
    }
    return GURL();
  }());
  return *continuity_url;
}

// Retrieves the GURL for the Web Continuity Originating Host, defaulting to the
// Guest URL's origin.
GURL GetGlicWebContinuityOriginatingHostUrl() {
  static base::NoDestructor<std::string> last_pref_value("");
  static base::NoDestructor<GURL> continuity_url;

  std::string current_pref_value = g_browser_process->local_state()->GetString(
      prefs::kGlicWebContinuityOriginatingHostUrlPreset);

  if (current_pref_value != *last_pref_value || !continuity_url->is_valid()) {
    *last_pref_value = current_pref_value;

    std::string continuity_url_str =
        features::kGlicWebContinuityOriginatingHost.Get();
    if (!continuity_url_str.empty()) {
      GURL url(continuity_url_str);
      if (url.is_valid()) {
        *continuity_url = url;
        return *continuity_url;
      }
    }

    if (!current_pref_value.empty()) {
      GURL url(current_pref_value);
      if (url.is_valid()) {
        *continuity_url = url;
        return *continuity_url;
      }
    }

    std::string guest_url_str = features::kGlicGuestURL.Get();
    if (!guest_url_str.empty()) {
      GURL url(guest_url_str);
      if (url.is_valid()) {
        *continuity_url = url.GetWithEmptyPath();
        return *continuity_url;
      }
    }
    *continuity_url = GURL();
  }
  return *continuity_url;
}

// static
void GlicNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  // We won't create a throttle if neither feature is enabled.
  if (!base::FeatureList::IsEnabled(features::kGlicGeminiContinueURLRedirect) &&
      !base::FeatureList::IsEnabled(features::kGlicWebContinuity)) {
    return;
  }
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  content::WebContents* web_contents = handle.GetWebContents();
  if (!web_contents) {
    return;
  }
  if (!handle.IsInPrimaryMainFrame() ||
      !tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    return;
  }
  const GURL& url = handle.GetURL();
  const GURL& web_continuity_url = GetGlicWebContinuityUrl();
  const GURL& web_continuity_originating_host_url =
      GetGlicWebContinuityOriginatingHostUrl();

  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  bool url_match = url.ReplaceComponents(replacements) ==
                   web_continuity_url.ReplaceComponents(replacements);

  if (url_match) {
    const std::optional<url::Origin>& initiator_origin =
        handle.GetInitiatorOrigin();
    if (initiator_origin.has_value()) {
      bool initiator_host_match =
          initiator_origin->GetURL().scheme() ==
              web_continuity_originating_host_url.scheme() &&
          initiator_origin->GetURL().host() ==
              web_continuity_originating_host_url.host();
      if (initiator_host_match) {
        registry.AddThrottle(
            std::make_unique<GlicNavigationThrottle>(registry));
      }
    }
  }
}

GlicNavigationThrottle::GlicNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

GlicNavigationThrottle::~GlicNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GlicNavigationThrottle::WillStartRequest() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return PROCEED;
  }

  GURL url = navigation_handle()->GetURL();
  std::optional<std::string> cid;
  std::optional<std::string> target_url_str;
  std::optional<std::string> turn_id;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bool is_glic_enabled = GlicEnabling::IsEnabledForProfile(profile);

  size_t max_cid_length = features::kGlicWebContinuityMaxCIDLength.Get();
  size_t max_target_url_length =
      features::kGlicWebContinuityMaxTargetUrlLength.Get();
  size_t max_turn_id_length = features::kGlicWebContinuityMaxTurnIdLength.Get();

  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == "cid") {
      std::string unescaped_cid = it.GetUnescapedValue();
      if (unescaped_cid.length() > max_cid_length) {
        LogCaptureResult(is_glic_enabled,
                         GeminiNavigationCaptureResult::kCIDTooLong);
        return PROCEED;
      }
      cid = unescaped_cid;
    } else if (it.GetKey() == "targetUrl") {
      target_url_str = it.GetUnescapedValue();
      if (target_url_str->length() > max_target_url_length) {
        LogCaptureResult(is_glic_enabled,
                         GeminiNavigationCaptureResult::kTargetUrlTooLong);
        return PROCEED;
      }
    } else if (it.GetKey() == "turnId") {
      turn_id = it.GetUnescapedValue();
      if (turn_id->length() > max_turn_id_length) {
        LogCaptureResult(is_glic_enabled,
                         GeminiNavigationCaptureResult::kTurnIdTooLong);
        return PROCEED;
      }
    }
  }

  if (!target_url_str) {
    LogCaptureResult(is_glic_enabled,
                     GeminiNavigationCaptureResult::kNoTargetUrl);
    return PROCEED;
  }
  GURL target_url(*target_url_str);
  if (!target_url.is_valid()) {
    LogCaptureResult(is_glic_enabled,
                     GeminiNavigationCaptureResult::kInvalidUrl);
    return PROCEED;
  }
  if (!target_url.SchemeIs(url::kHttpsScheme)) {
    LogCaptureResult(is_glic_enabled,
                     GeminiNavigationCaptureResult::kNonHttpsScheme);
    return PROCEED;
  }

  GlicKeyedService* glic_service = GlicKeyedService::Get(profile);
  if (is_glic_enabled && glic_service &&
      base::FeatureList::IsEnabled(features::kGlicWebContinuity)) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents);
    if (tab && tab->GetBrowserWindowInterface()) {
      glic::Target target(*tab);
      if (cid) {
        target.conversation = glic::ConversationId(*cid, turn_id);
      }
      GlicInvokeOptions options(
          std::move(target), glic::mojom::InvocationSource::kNavigationCapture);
      glic_service->Invoke(std::move(options));
    }
  }

  // Navigate to the target URL.
  NavigateParams params(profile, target_url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  params.source_contents = web_contents;
  params.initiator_origin = navigation_handle()->GetInitiatorOrigin();
  params.is_renderer_initiated = navigation_handle()->IsRendererInitiated();
  params.user_gesture = navigation_handle()->HasUserGesture();
  params.original_user_gesture = navigation_handle()->HasUserGesture();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce([](NavigateParams params) { Navigate(&params); },
                     std::move(params)));
  LogCaptureResult(is_glic_enabled, GeminiNavigationCaptureResult::kSuccess);
  return CANCEL;
}

const char* GlicNavigationThrottle::GetNameForLogging() {
  return "GlicNavigationThrottle";
}

void GlicNavigationThrottle::LogCaptureResult(
    bool is_glic_enabled,
    glic::GeminiNavigationCaptureResult result) {
  bool is_web_continuity_enabled =
      base::FeatureList::IsEnabled(features::kGlicWebContinuity);
  std::string_view status_string = is_glic_enabled && is_web_continuity_enabled
                                       ? "GlicWebContinuityFeatureEnabled"
                                       : "GlicWebContinuityFeatureDisabled";
  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.NavigationCapture.", status_string}), result);
}

}  // namespace glic
