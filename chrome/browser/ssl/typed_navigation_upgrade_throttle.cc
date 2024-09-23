// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/security_interstitials/core/omnibox_https_upgrade_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

using security_interstitials::omnibox_https_upgrades::Event;
using security_interstitials::omnibox_https_upgrades::kEventHistogram;

namespace {

// Delay before falling back to the HTTP URL.
// This can be changed in tests.
// - If the HTTPS load finishes successfully during this time, the timer is
//   cleared and no more work is done.
// - Otherwise, a new navigation to the the fallback HTTP URL is started.
constexpr base::FeatureParam<base::TimeDelta> kFallbackDelay{
    &omnibox::kDefaultTypedNavigationsToHttps,
    omnibox::kDefaultTypedNavigationsToHttpsTimeoutParam, base::Seconds(3)};

int g_https_port_for_testing = 0;

// Used to compute the fallback URL from the https URL.
int g_http_port_for_testing = 0;

void RecordUMA(Event event) {
  base::UmaHistogramEnumeration(kEventHistogram, event);
}

GURL GetHttpUrl(const GURL& url, int http_fallback_port_for_testing) {
  DCHECK_EQ(url::kHttpsScheme, url.scheme());
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);

  // This needs to be in scope when ReplaceComponents() is called:
  const std::string port_str =
      base::NumberToString(http_fallback_port_for_testing);
  if (http_fallback_port_for_testing) {
    // We'll only get here in tests. Tests should always have a non-default
    // port on the input text.
    DCHECK(!url.port().empty());
    replacements.SetPortStr(port_str);
  }
  return url.ReplaceComponents(replacements);
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
TypedNavigationUpgradeThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if the omnibox added https as the default scheme for this navigation.
  // If not, no need to create the throttle.
  if (!IsNavigationUsingHttpsAsDefaultScheme(handle)) {
    return nullptr;
  }
  // Typed main frame navigations can only be GET requests.
  DCHECK(!handle->IsPost());

  return base::WrapUnique(new TypedNavigationUpgradeThrottle(handle));
}

// static
bool TypedNavigationUpgradeThrottle::IsNavigationUsingHttpsAsDefaultScheme(
    content::NavigationHandle* handle) {
  content::NavigationUIData* ui_data = handle->GetNavigationUIData();
  // UI data can be null in the case of navigations to interstitials.
  if (!ui_data) {
    return false;
  }
  // Only handle HTTPS navigations typed in the omnibox. If a navigation has
  // HTTP URL, either the omnibox didn't upgrade the navigation to HTTPS, or it
  // previously upgraded and we fell back to HTTP so there is no need to
  // observe again.
  // TODO(crbug.com/40162528): There are cases where we don't currently upgrade
  // even though we probably should. Make a decision for the ones listed in the
  // bug and potentially identify more.
  bool is_using_https_as_default_scheme =
      static_cast<ChromeNavigationUIData*>(ui_data)
          ->is_using_https_as_default_scheme();
  return is_using_https_as_default_scheme && handle->IsInPrimaryMainFrame() &&
         !handle->IsSameDocument() &&
         handle->GetURL().SchemeIs(url::kHttpsScheme);
}

TypedNavigationUpgradeThrottle::~TypedNavigationUpgradeThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
TypedNavigationUpgradeThrottle::WillStartRequest() {
  DCHECK_EQ(url::kHttpsScheme, navigation_handle()->GetURL().scheme());
  RecordUMA(Event::kHttpsLoadStarted);
  timer_.Start(FROM_HERE, kFallbackDelay.Get(), this,
               &TypedNavigationUpgradeThrottle::OnHttpsLoadTimeout);
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TypedNavigationUpgradeThrottle::WillFailRequest() {
  DCHECK_EQ(url::kHttpsScheme, navigation_handle()->GetURL().scheme());
  // Cancel the request, stop the timer and fall back to HTTP in case of SSL
  // errors or other net/ errors.
  timer_.Stop();
  // If there was no certificate error, SSLInfo will be empty.
  const net::SSLInfo info =
      navigation_handle()->GetSSLInfo().value_or(net::SSLInfo());
  int cert_status = info.cert_status;
  if (!net::IsCertStatusError(cert_status) &&
      navigation_handle()->GetNetErrorCode() == net::OK) {
    return content::NavigationThrottle::PROCEED;
  }

  if (net::IsCertStatusError(cert_status)) {
    RecordUMA(Event::kHttpsLoadFailedWithCertError);
  } else if (navigation_handle()->GetNetErrorCode() != net::OK) {
    RecordUMA(Event::kHttpsLoadFailedWithNetError);
  }

  // Fallback to Http without stopping the navigation. The return value of this
  // method takes care of that, and we don't need to call WebContents::Stop on a
  // navigation that's already about to fail.
  FallbackToHttp(false);

  // Do not add any code after here, |this| is deleted.
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

content::NavigationThrottle::ThrottleCheckResult
TypedNavigationUpgradeThrottle::WillRedirectRequest() {
  RecordUMA(Event::kRedirected);
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TypedNavigationUpgradeThrottle::WillProcessResponse() {
  // If we got here, the HTTPS load succeeded. The final URL may be HTTPS or
  // HTTP (if the upgrade attempt redirected to HTTP). In any case, we are done,
  // so stop the timer.
  RecordUMA(Event::kHttpsLoadSucceeded);
  timer_.Stop();
  return content::NavigationThrottle::PROCEED;
}

const char* TypedNavigationUpgradeThrottle::GetNameForLogging() {
  return "TypedNavigationUpgradeThrottle";
}

// static
int TypedNavigationUpgradeThrottle::GetHttpsPortForTesting() {
  return g_https_port_for_testing;
}

// static
void TypedNavigationUpgradeThrottle::SetHttpsPortForTesting(
    int https_port_for_testing) {
  g_https_port_for_testing = https_port_for_testing;
}

// static
void TypedNavigationUpgradeThrottle::SetHttpPortForTesting(
    int http_port_for_testing) {
  g_http_port_for_testing = http_port_for_testing;
}

TypedNavigationUpgradeThrottle::TypedNavigationUpgradeThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle),
      http_url_(GetHttpUrl(handle->GetURL(), g_http_port_for_testing)) {}

void TypedNavigationUpgradeThrottle::OnHttpsLoadTimeout() {
  RecordUMA(Event::kHttpsLoadTimedOut);
  // Stop the current navigation and load the HTTP URL. We explicitly stop the
  // navigation here as opposed to WillFailRequest because the timeout happens
  // in the middle of a navigation where we can't return a ThrottleCheckResult.
  FallbackToHttp(true);

  // Once the fallback navigation starts, |this| will be deleted. Be careful
  // adding code here -- any async task posted hereafter may never run.
}

void TypedNavigationUpgradeThrottle::FallbackToHttp(bool stop_navigation) {
  DCHECK_EQ(url::kHttpScheme, http_url_.scheme()) << http_url_;
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.url = http_url_;

  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  // According to crbug.com/1058303, web_contents could be null but we don't
  // want to speculatively handle that case here, so just DCHECK for now.
  DCHECK(web_contents);

  // Post a task to navigate to the fallback URL. We don't navigate
  // synchronously here, as starting a navigation within a navigation is
  // an antipattern.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> web_contents,
             const content::OpenURLParams& url_params, bool stop_navigation) {
            if (!web_contents)
              return;
            if (stop_navigation) {
              // This deletes the NavigationThrottle and NavigationHandle.
              web_contents->Stop();
            }
            web_contents->OpenURL(url_params,
                                  /*navigation_handle_callback=*/{});
          },
          web_contents->GetWeakPtr(), std::move(params), stop_navigation));

  // Once the fallback navigation starts, |this| will be deleted. Be careful
  // adding code here -- any async task posted hereafter may never run.
}
