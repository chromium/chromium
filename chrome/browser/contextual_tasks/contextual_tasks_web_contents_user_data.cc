// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
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
    return input_state_model_->AsWeakPtr();
  }

  content::WebContents* web_contents = &GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* service = AimEligibilityServiceFactory::GetForProfile(profile);
  const omnibox::SearchboxConfig* config =
      service ? service->GetSearchboxConfig() : nullptr;

  const signin::IdentityManager* identity_manager =
      profile ? IdentityManagerFactory::GetForProfile(profile) : nullptr;
  bool has_primary_account =
      identity_manager &&
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);

  GURL url = web_contents->GetLastCommittedURL();
  bool is_off_the_record = profile->IsOffTheRecord();

  input_state_model_ = std::make_unique<contextual_search::InputStateModel>(
      session_handle, config ? *config : omnibox::SearchboxConfig(), url,
      is_off_the_record, has_primary_account);

  return input_state_model_->AsWeakPtr();
}

}  // namespace contextual_tasks
