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

void AimUserAgentTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigation_handle->HasCommitted() && is_ua_overridden_by_aim_) {
    web_contents()->SetUserAgentOverride(original_ua_override_,
                                         /*override_in_new_tabs=*/false);
    is_ua_overridden_by_aim_ = false;
    original_ua_override_ = blink::UserAgentOverride();
  }
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
  const bool is_cobrowse_eligible =
      aim_eligibility_service && aim_eligibility_service->IsCobrowseEligible();

  const GURL& target_url = navigation_handle->GetURL();
  // Override the User-Agent if cobrowse is eligible and either:
  // 1. A tab WebContents navigates to an AI URL (e.g., g.ai or AIM).
  // 2. The WebContents is hosted within the contextual tasks side panel.
  const bool should_override =
      is_cobrowse_eligible &&
      (ui_service->IsAiUrl(target_url) ||
       ui_service->IsSidePanelOpenAndRequestInSidePanel(web_contents()));

  if (should_override) {
    if (!is_ua_overridden_by_aim_) {
      original_ua_override_ = web_contents()->GetUserAgentOverride();

      blink::UserAgentOverride ua_override;
      if (!original_ua_override_.ua_string_override.empty()) {
        ua_override.ua_string_override =
            original_ua_override_.ua_string_override + " " +
            GetContextualTasksUserAgentSuffix();
        ua_override.ua_metadata_override =
            original_ua_override_.ua_metadata_override;
      } else {
        ua_override.ua_string_override =
            embedder_support::GetUserAgent() + " " +
            GetContextualTasksUserAgentSuffix();
        // Populate metadata override to preserve User Agent Client Hints
        // (Sec-CH-UA-*). Leaving it nullopt would suppress Client Hints.
        ua_override.ua_metadata_override =
            embedder_support::GetUserAgentMetadata();
      }

      web_contents()->SetUserAgentOverride(ua_override,
                                           /*override_in_new_tabs=*/false);
      is_ua_overridden_by_aim_ = true;
    }
    // Only call SetIsOverridingUserAgent if we are not in a redirect.
    // Calling it during redirect crashes (b:543901499) due to
    // CHECK(!ua_change_requires_reload_).
    if (!navigation_handle->WasServerRedirect()) {
      navigation_handle->SetIsOverridingUserAgent(true);
    }
  } else {
    if (is_ua_overridden_by_aim_) {
      web_contents()->SetUserAgentOverride(original_ua_override_,
                                           /*override_in_new_tabs=*/false);
      // Only call SetIsOverridingUserAgent if we are not in a redirect.
      // Calling it during redirect crashes (b:543901499) due to
      // CHECK(!ua_change_requires_reload_).
      if (!navigation_handle->WasServerRedirect()) {
        navigation_handle->SetIsOverridingUserAgent(
            !original_ua_override_.ua_string_override.empty());
      }
      is_ua_overridden_by_aim_ = false;
      original_ua_override_ = blink::UserAgentOverride();
    }
  }
}

}  // namespace contextual_tasks
