// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_tab_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/login_detection/login_detection_keyed_service.h"
#include "chrome/browser/login_detection/login_detection_keyed_service_factory.h"
#include "chrome/browser/login_detection/login_detection_prefs.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/prefs/pref_service.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace login_detection {

// The only purpose of this class currently is to be friended by the
// UkmRecorder. It therefore cannot be in the anonymous namespace.
class IdentityProviderMetrics {
 public:
  // Gets the ukm source id for a web identity provider.
  static ukm::SourceId GetUkmSourceIdForWebIdentityFromScope(
      const GURL& provider) {
    return ukm::UkmRecorder::GetSourceIdForWebIdentityFromScope(
        base::PassKey<IdentityProviderMetrics>(), provider);
  }
};

namespace {

PrefService* GetPrefs(content::WebContents* web_contents) {
  return Profile::FromBrowserContext(web_contents->GetBrowserContext())
      ->GetPrefs();
}

void RecordLoginDetectionMetrics(LoginDetectionType type,
                                 const std::optional<GURL>& provider,
                                 ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration("Login.PageLoad.DetectionType", type);

  if (type == LoginDetectionType::kNoLogin) {
    return;
  }
  password_manager::metrics_util::RecordBrowserAssistedLogin(
      password_manager::metrics_util::BrowserAssistedLoginType::kNonFedCmOAuth);
  ukm::builders::LoginDetectionV2 builder(ukm_source_id);
  builder.SetPage_LoginType(static_cast<int64_t>(type))
      .Record(ukm::UkmRecorder::Get());

  if (!provider) {
    return;
  }

  ukm::builders::LoginDetectionV2IdentityProvider identity_provider_builder(
      IdentityProviderMetrics::GetUkmSourceIdForWebIdentityFromScope(
          *provider));
  identity_provider_builder.SetPage_LoginType(static_cast<int64_t>(type))
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace

// static
void LoginDetectionTabHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents->GetBrowserContext() ||
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return;
  }
  if (!IsLoginDetectionFeatureEnabled())
    return;
  LoginDetectionTabHelper::CreateForWebContents(web_contents);
}

LoginDetectionTabHelper::LoginDetectionTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<LoginDetectionTabHelper>(*web_contents),
      oauth_login_detector_(std::make_unique<OAuthLoginDetector>()),
      ukm_source_id_(
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()) {
  DCHECK(IsLoginDetectionFeatureEnabled());
}

LoginDetectionTabHelper::~LoginDetectionTabHelper() = default;

void LoginDetectionTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;
  if (!navigation_handle->HasCommitted())
    return;
  if (navigation_handle->IsSameDocument())
    return;

  GURL url = navigation_handle->GetURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return;

  GURL prev_navigation_url;
  if (auto* prev_navigation =
          web_contents()->GetController().GetEntryAtOffset(-1)) {
    prev_navigation_url = prev_navigation->GetURL();
  }

  // Check if OAuth login on the site happened now. This check should happen
  // first before other checks since this could be a repeated OAuth login and
  // the time of login will be updated.
  if (auto login_info = oauth_login_detector_->GetSuccessfulLoginFlowSite(
          prev_navigation_url, navigation_handle->GetRedirectChain())) {
    ProcessNewSignedInSite(login_info->oauth_requestor_site);
    RecordLoginDetectionMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow,
                                login_info->oauth_provider_site,
                                navigation_handle->GetNextPageUkmSourceId());
    return;
  }

  RecordLoginDetectionMetrics(LoginDetectionType::kNoLogin, std::nullopt,
                              navigation_handle->GetNextPageUkmSourceId());
}

void LoginDetectionTabHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (disposition != WindowOpenDisposition::NEW_POPUP)
    return;
  // DidOpenRequestedURL may be called before `new_contents` has been added to
  // the tab strip, and as such tab helpers may not yet have been initialized.
  // Explicitly instantiate the login detector for the `new_contents` here to
  // ensure the popup is registered correctly.
  LoginDetectionTabHelper::CreateForWebContents(new_contents);
  if (auto* new_tab_helper =
          LoginDetectionTabHelper::FromWebContents(new_contents)) {
    new_tab_helper->DidOpenAsPopUp(web_contents()->GetLastCommittedURL());
  }
}

void LoginDetectionTabHelper::DidOpenAsPopUp(
    const GURL& opener_navigation_url) {
  oauth_login_detector_->DidOpenAsPopUp(opener_navigation_url);
}

void LoginDetectionTabHelper::PrimaryPageChanged(content::Page& page) {
  ukm_source_id_ = page.GetMainDocument().GetPageUkmSourceId();
}

void LoginDetectionTabHelper::WebContentsDestroyed() {
  if (auto login_info = oauth_login_detector_->GetPopUpLoginFlowSite()) {
    ProcessNewSignedInSite(login_info->oauth_requestor_site);
    RecordLoginDetectionMetrics(
        LoginDetectionType::kOauthPopUpFirstTimeLoginFlow,
        login_info->oauth_provider_site, ukm_source_id_);
  }
  oauth_login_detector_.reset();
}

void LoginDetectionTabHelper::ProcessNewSignedInSite(
    const GURL& signedin_site) {
  prefs::SaveSiteToOAuthSignedInList(GetPrefs(web_contents()), signedin_site);
  site_isolation::SiteIsolationPolicy::IsolateNewOAuthURL(
      web_contents()->GetBrowserContext(), signedin_site);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoginDetectionTabHelper);

}  // namespace login_detection
