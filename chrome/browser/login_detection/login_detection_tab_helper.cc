// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "components/prefs/pref_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace login_detection {

namespace {

PrefService* GetPrefs(content::WebContents* web_contents) {
  return Profile::FromBrowserContext(web_contents->GetBrowserContext())
      ->GetPrefs();
}

void RecordLoginDetectionMetrics(LoginDetectionType type,
                                 ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration("Login.PageLoad.DetectionType", type);
  ukm::builders::LoginDetection builder(ukm_source_id);
  builder.SetPage_LoginType(static_cast<int64_t>(type))
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
      oauth_login_detector_(std::make_unique<OAuthLoginDetector>()) {
  DCHECK(IsLoginDetectionFeatureEnabled());
}

LoginDetectionTabHelper::~LoginDetectionTabHelper() = default;

void LoginDetectionTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);
  if (!navigation_handle->IsInMainFrame())
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
  if (auto signedin_site = oauth_login_detector_->GetSuccessfulLoginFlowSite(
          prev_navigation_url, navigation_handle->GetRedirectChain())) {
    prefs::SaveSiteToOAuthSignedInList(GetPrefs(web_contents()),
                                       *signedin_site);
    RecordLoginDetectionMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow,
                                navigation_handle->GetNextPageUkmSourceId());
    return;
  }

  LoginDetectionKeyedService* login_detection_keyed_service =
      LoginDetectionKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (!login_detection_keyed_service)
    return;

  RecordLoginDetectionMetrics(
      login_detection_keyed_service->GetPersistentLoginDetection(url),
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
  if (auto* new_tab_helper =
          LoginDetectionTabHelper::FromWebContents(new_contents)) {
    new_tab_helper->DidOpenAsPopUp(web_contents()->GetLastCommittedURL());
  }
}

void LoginDetectionTabHelper::DidOpenAsPopUp(
    const GURL& opener_navigation_url) {
  oauth_login_detector_->DidOpenAsPopUp(opener_navigation_url);
}

void LoginDetectionTabHelper::WebContentsDestroyed() {
  if (auto signedin_site = oauth_login_detector_->GetPopUpLoginFlowSite()) {
    RecordLoginDetectionMetrics(
        LoginDetectionType::kOauthPopUpFirstTimeLoginFlow,
        ukm::GetSourceIdForWebContentsDocument(web_contents()));
    prefs::SaveSiteToOAuthSignedInList(GetPrefs(web_contents()),
                                       *signedin_site);
  }
  oauth_login_detector_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoginDetectionTabHelper)

}  // namespace login_detection
