// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_tab_observer.h"

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "privacy_sandbox_survey_desktop_controller.h"
#include "privacy_sandbox_survey_desktop_controller_factory.h"
#include "privacy_sandbox_tab_observer.h"

namespace privacy_sandbox {

PrivacySandboxTabObserver::PrivacySandboxTabObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PrivacySandboxTabObserver::~PrivacySandboxTabObserver() = default;

void PrivacySandboxTabObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  // Only valid top frame navigations are considered for showing notices and
  // HATs.
  if (!navigation_handle || !navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // HATs
  if (IsNewTabPage()) {
    auto* desktop_survey_controller =
        PrivacySandboxSurveyDesktopControllerFactory::GetForProfile(profile);
    if (desktop_survey_controller) {
      desktop_survey_controller->MaybeShowSentimentSurvey();
      desktop_survey_controller->OnNewTabPageSeen();
    }
  }
}

bool PrivacySandboxTabObserver::IsNewTabPage() {
  return web_contents()->GetLastCommittedURL() ==
             chrome::kChromeUINewTabPageURL ||
         web_contents()->GetLastCommittedURL() == chrome::kChromeUINewTabURL;
}

}  // namespace privacy_sandbox
