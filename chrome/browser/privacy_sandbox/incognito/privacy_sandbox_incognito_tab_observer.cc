// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_incognito_tab_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "privacy_sandbox_incognito_survey_service.h"
#include "privacy_sandbox_incognito_survey_service_factory.h"

namespace privacy_sandbox {

PrivacySandboxIncognitoTabObserver::PrivacySandboxIncognitoTabObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PrivacySandboxIncognitoTabObserver::~PrivacySandboxIncognitoTabObserver() =
    default;

void PrivacySandboxIncognitoTabObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->IsInPrimaryMainFrame() &&
      IsNewTabPage(validated_url)) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    auto* survey_service =
        PrivacySandboxIncognitoSurveyServiceFactory::GetForProfile(profile);
    if (survey_service) {
      // We cannot show the ACT survey on DidFinishNavigation, because is uses
      // the HaTS navigation behavior for delayed surveys which uses
      // DidFinishNavigation to check for valid navigations.
      survey_service->MaybeShowActSurvey(web_contents());
    }
  }
}

bool PrivacySandboxIncognitoTabObserver::IsNewTabPage(const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  // On Android, the new tab page has a different URL.
  if (url == chrome::kChromeUINativeNewTabURL) {
    return true;
  }
#endif

  return url == chrome::kChromeUINewTabPageURL ||
         url == chrome::kChromeUINewTabURL;
}

}  // namespace privacy_sandbox
