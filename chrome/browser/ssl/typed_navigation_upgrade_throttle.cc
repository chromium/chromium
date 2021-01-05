// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

bool IsNavigationUsingHttpsAsDefaultScheme(content::NavigationHandle* handle) {
  content::NavigationUIData* ui_data = handle->GetNavigationUIData();
  // UI data can be null in the case of navigations to interstitials.
  if (!ui_data) {
    return false;
  }
  return static_cast<ChromeNavigationUIData*>(ui_data)
      ->is_using_https_as_default_scheme();
}

void RecordUMA(TypedNavigationUpgradeThrottle::Event event) {
  base::UmaHistogramEnumeration(TypedNavigationUpgradeThrottle::kHistogramName,
                                event);
}

// Used to scope the posted navigation task to the lifetime of |web_contents|.
// We can start a new navigation from inside the throttle using this class.
class TypedNavigationUpgradeLifetimeHelper
    : public content::WebContentsUserData<
          TypedNavigationUpgradeLifetimeHelper> {
 public:
  explicit TypedNavigationUpgradeLifetimeHelper(
      content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  base::WeakPtr<TypedNavigationUpgradeLifetimeHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void Navigate(const content::OpenURLParams& url_params) {
    web_contents_->OpenURL(url_params);
  }

 private:
  friend class content::WebContentsUserData<
      TypedNavigationUpgradeLifetimeHelper>;

  content::WebContents* const web_contents_;
  base::WeakPtrFactory<TypedNavigationUpgradeLifetimeHelper> weak_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TypedNavigationUpgradeLifetimeHelper)

GURL GetHttpUrl(const GURL& url) {
  DCHECK_EQ(url::kHttpsScheme, url.scheme());
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  return url.ReplaceComponents(replacements);
}

}  // namespace

// static
const char TypedNavigationUpgradeThrottle::kHistogramName[] =
    "TypedNavigationUpgradeThrottle.Event";

// static
std::unique_ptr<content::NavigationThrottle>
TypedNavigationUpgradeThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only observe HTTPS navigations typed in the omnibox. If a navigation has
  // HTTP URL, either the omnibox didn't upgrade the navigation to HTTPS, or it
  // previously upgraded and we fell back to HTTP so there is no need to
  // observe again.
  // TODO(crbug.com/1161620): There are cases where we don't currently upgrade
  // even though we probably should. Make a decision for the ones listed in the
  // bug and potentially identify more.
  if (!handle->IsInMainFrame() || handle->IsSameDocument() ||
      !handle->GetURL().SchemeIs(url::kHttpsScheme) ||
      handle->GetWebContents()->IsPortal() ||
      !ui::PageTransitionCoreTypeIs(handle->GetPageTransition(),
                                    ui::PAGE_TRANSITION_TYPED) ||
      !ui::PageTransitionIsNewNavigation(handle->GetPageTransition())) {
    return nullptr;
  }
  // Typed main frame navigations can only be GET requests.
  DCHECK(!handle->IsPost());

  // Check if the omnibox added https as the default scheme for this navigation.
  // If not, no need to create the throttle.
  if (!IsNavigationUsingHttpsAsDefaultScheme(handle)) {
    return nullptr;
  }

  return base::WrapUnique(new TypedNavigationUpgradeThrottle(handle));
}

TypedNavigationUpgradeThrottle::~TypedNavigationUpgradeThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
TypedNavigationUpgradeThrottle::WillStartRequest() {
  DCHECK_EQ(url::kHttpsScheme, navigation_handle()->GetURL().scheme());
  RecordUMA(Event::kHttpsLoadStarted);
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TypedNavigationUpgradeThrottle::WillFailRequest() {
  DCHECK_EQ(url::kHttpsScheme, navigation_handle()->GetURL().scheme());
  // Cancel and fall back to HTTP in case of SSL errors or other net/ errors.
  // If there was no certificate error, SSLInfo will be empty.
  const net::SSLInfo info =
      navigation_handle()->GetSSLInfo().value_or(net::SSLInfo());
  int cert_status = info.cert_status;
  if (net::IsCertStatusError(cert_status)) {
    RecordUMA(Event::kHttpsLoadFailedWithCertError);
    FallbackToHttp();
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  if (navigation_handle()->GetNetErrorCode() != net::OK) {
    RecordUMA(Event::kHttpsLoadFailedWithNetError);
    FallbackToHttp();
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
TypedNavigationUpgradeThrottle::WillProcessResponse() {
  DCHECK_EQ(url::kHttpsScheme, navigation_handle()->GetURL().scheme());
  // If we got here, HTTPS load succeeded.
  RecordUMA(Event::kHttpsLoadSucceeded);
  return content::NavigationThrottle::PROCEED;
}

const char* TypedNavigationUpgradeThrottle::GetNameForLogging() {
  return "TypedNavigationUpgradeThrottle";
}

// static
bool TypedNavigationUpgradeThrottle::
    ShouldIgnoreInterstitialBecauseNavigationDefaultedToHttps(
        content::NavigationHandle* handle) {
  DCHECK_EQ(url::kHttpsScheme, handle->GetURL().scheme());
  return base::FeatureList::IsEnabled(
             omnibox::kDefaultTypedNavigationsToHttps) &&
         IsNavigationUsingHttpsAsDefaultScheme(handle);
}

TypedNavigationUpgradeThrottle::TypedNavigationUpgradeThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

void TypedNavigationUpgradeThrottle::FallbackToHttp() {
  const GURL http_url = GetHttpUrl(navigation_handle()->GetURL());
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.url = http_url;

  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  // According to crbug.com/1058303, web_contents could be null but we don't
  // want to speculatively handle that case here, so just DCHECK for now.
  DCHECK(web_contents);

  // Post a task to navigate to the fallback URL. We don't navigate
  // synchronously here, as starting a navigation within a navigation is
  // an antipattern. Use a helper object scoped to the WebContents lifetime to
  // scope the navigation task to the WebContents lifetime.
  // See PDFIFrameNavigationThrottle::LoadPlaceholderHTML() for another use of
  // this pattern.
  // CreateForWebContents is a no-op if there is already a helper.
  TypedNavigationUpgradeLifetimeHelper::CreateForWebContents(web_contents);
  TypedNavigationUpgradeLifetimeHelper* helper =
      TypedNavigationUpgradeLifetimeHelper::FromWebContents(web_contents);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TypedNavigationUpgradeLifetimeHelper::Navigate,
                                helper->GetWeakPtr(), std::move(params)));
}
