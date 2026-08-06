// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/aim_user_agent_tab_helper.h"

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace contextual_tasks {

WEB_CONTENTS_USER_DATA_KEY_IMPL(AimUserAgentTabHelper);

AimUserAgentTabHelper::AimUserAgentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AimUserAgentTabHelper>(*web_contents) {}

AimUserAgentTabHelper::~AimUserAgentTabHelper() = default;

void AimUserAgentTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  UpdateUserAgentForNavigation(navigation_handle);
}

void AimUserAgentTabHelper::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  UpdateUserAgentForNavigation(navigation_handle);
}

void AimUserAgentTabHelper::UpdateUserAgentForNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!IsContextualTasksUIEnabled()) {
    return;
  }

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!web_contents()) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile) {
    return;
  }

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile);
  if (!ui_service) {
    return;
  }

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  const bool is_aim_eligible =
      aim_eligibility_service && aim_eligibility_service->IsAimEligible();

  const GURL& target_url = navigation_handle->GetURL();
  if (is_aim_eligible &&
      (ui_service->IsAiUrl(target_url) ||
       ui_service->IsSidePanelOpenAndRequestInSidePanel(web_contents()))) {
    blink::UserAgentOverride ua_override;
    ua_override.ua_string_override = embedder_support::GetUserAgent() + " " +
                                     GetContextualTasksUserAgentSuffix();
    // Populate metadata override to preserve User Agent Client Hints
    // (Sec-CH-UA-*). Leaving it nullopt would suppress Client Hints.
    ua_override.ua_metadata_override = embedder_support::GetUserAgentMetadata();

    web_contents()->SetUserAgentOverride(ua_override,
                                         /*override_in_new_tabs=*/false);
    navigation_handle->SetIsOverridingUserAgent(true);
    is_ua_overridden_by_aim_ = true;
  } else if (is_ua_overridden_by_aim_) {
    web_contents()->SetUserAgentOverride(blink::UserAgentOverride(),
                                         /*override_in_new_tabs=*/false);
    navigation_handle->SetIsOverridingUserAgent(false);
    is_ua_overridden_by_aim_ = false;
  }
}

}  // namespace contextual_tasks
