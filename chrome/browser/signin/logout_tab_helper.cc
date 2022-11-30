// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/logout_tab_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(LogoutTabHelper);

LogoutTabHelper::LogoutTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<LogoutTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents) {}

LogoutTabHelper::~LogoutTabHelper() = default;

void LogoutTabHelper::PrimaryPageChanged(content::Page& page) {
  if (page.GetMainDocument().IsErrorDocument()) {
    // Failed to load the logout page, fallback to local signout.
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    IdentityManagerFactory::GetForProfile(profile)
        ->GetAccountsMutator()
        ->RemoveAllAccounts(signin_metrics::SourceForRefreshTokenOperation::
                                kLogoutTabHelper_PrimaryPageChanged);
  }

  // Delete this.
  web_contents()->RemoveUserData(UserDataKey());
}
