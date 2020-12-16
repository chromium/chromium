// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_tab_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/login_detection/login_detection_prefs.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
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

void RecordLoginDetectionMetrics(
    LoginDetectionTabHelper::LoginDetectionType type,
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
    : content::WebContentsObserver(web_contents) {
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

  // Check if OAuth login on the site happened now. This check should happen
  // first before other checks since this could be a repeated OAuth login and
  // the time of login will be updated.
  for (const auto& redirect_url : navigation_handle->GetRedirectChain()) {
    if (oauth_login_detector_.CheckSuccessfulLoginFlow(redirect_url)) {
      prefs::SaveSiteToOAuthSignedInList(GetPrefs(web_contents()),
                                         redirect_url);
      RecordLoginDetectionMetrics(LoginDetectionType::kOauthFirstTimeLoginFlow,
                                  navigation_handle->GetNextPageUkmSourceId());
      return;
    }
  }

  // Check if OAuth login for this site was detected earlier, and remembered
  // in prefs.
  if (prefs::IsSiteInOAuthSignedInList(GetPrefs(web_contents()), url)) {
    RecordLoginDetectionMetrics(LoginDetectionType::kOauthLogin,
                                navigation_handle->GetNextPageUkmSourceId());
    return;
  }

  RecordLoginDetectionMetrics(LoginDetectionType::kNoLogin,
                              navigation_handle->GetNextPageUkmSourceId());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoginDetectionTabHelper)

}  // namespace login_detection
