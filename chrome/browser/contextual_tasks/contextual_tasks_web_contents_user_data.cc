// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksWebContentsUserData::ContextualTasksWebContentsUserData(
    content::WebContents* contents)
    : content::WebContentsUserData<ContextualTasksWebContentsUserData>(
          *contents) {}

ContextualTasksWebContentsUserData::~ContextualTasksWebContentsUserData() =
    default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualTasksWebContentsUserData);

base::WeakPtr<contextual_search::InputStateModel>
ContextualTasksWebContentsUserData::GetOrCreateInputStateModel(
    contextual_search::ContextualSearchSessionHandle& session_handle) {
  if (input_state_model_) {
    if (input_state_model_->session_handle() == &session_handle) {
      return input_state_model_->AsWeakPtr();
    }
    // The session handle changed (e.g. task switched). Destroy the old model
    // to start fresh and avoid using a stale session handle.
    input_state_model_.reset();
  }

  content::WebContents* web_contents = &GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* service = AimEligibilityServiceFactory::GetForProfile(profile);
  const omnibox::SearchboxConfig* config =
      service ? service->GetSearchboxConfig() : nullptr;

  auto* ui_service = profile
                         ? contextual_tasks::ContextualTasksUiServiceFactory::
                               GetForBrowserContext(profile)
                         : nullptr;
  GURL url = web_contents->GetLastCommittedURL();
  bool browser_identity_matches_aim_identity =
      ui_service && ui_service->IsSignedInToBrowserWithValidCredentials() &&
      ui_service->IsUrlForPrimaryAccount(url);

  bool is_off_the_record = profile->IsOffTheRecord();

  input_state_model_ = std::make_unique<contextual_search::InputStateModel>(
      session_handle, config ? *config : omnibox::SearchboxConfig(), url,
      is_off_the_record, browser_identity_matches_aim_identity);

  return input_state_model_->AsWeakPtr();
}

}  // namespace contextual_tasks
