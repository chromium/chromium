// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_incognito_tab_observer.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_handle.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "privacy_sandbox_whats_new_survey_service.h"
#include "privacy_sandbox_whats_new_survey_service_factory.h"
#endif

namespace privacy_sandbox {

PrivacySandboxIncognitoTabObserver::PrivacySandboxIncognitoTabObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PrivacySandboxIncognitoTabObserver::~PrivacySandboxIncognitoTabObserver() =
    default;

void PrivacySandboxIncognitoTabObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // What's new page is fully contained within a single iframe for all the
  // contents. The survey on the "What's New" page should only appear when the
  // *iframe's* content is fully loaded. This happens after the main frame's
  // DidFinishLoad. Thus, we ignore the main frame load event and only proceed
  // for subframe load events on the correct page.
  if (!render_frame_host->IsInPrimaryMainFrame() &&
      IsWhatsNewPage(
          render_frame_host->GetMainFrame()->GetLastCommittedURL())) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    auto* whats_new_survey_service =
        PrivacySandboxWhatsNewSurveyServiceFactory::GetForProfile(profile);
    if (whats_new_survey_service) {
      whats_new_survey_service->MaybeShowSurvey(web_contents());
    }
  }
#endif
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// constant kChromeUIWhatsNewURL is defined only for this three.
bool PrivacySandboxIncognitoTabObserver::IsWhatsNewPage(const GURL& url) {
  return url == chrome::kChromeUIWhatsNewURL;
}
#endif

}  // namespace privacy_sandbox
