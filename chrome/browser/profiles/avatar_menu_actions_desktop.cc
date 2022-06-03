// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_actions_desktop.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"

AvatarMenuActionsDesktop::AvatarMenuActionsDesktop() {
}

AvatarMenuActionsDesktop::~AvatarMenuActionsDesktop() {
}

// static
AvatarMenuActions* AvatarMenuActions::Create() {
  return new AvatarMenuActionsDesktop();
}

void AvatarMenuActionsDesktop::AddNewProfile(ProfileMetrics::ProfileAdd type) {
  // TODO: Remove dependency on Browser by delegating AddNewProfile and
  // and EditProfile actions.

  Browser* settings_browser = browser_;
  if (!settings_browser) {
    const Browser::CreateParams params(ProfileManager::GetLastUsedProfile(),
                                       true);
    settings_browser = Browser::Create(params);
  }
  chrome::ShowSettingsSubPage(settings_browser, chrome::kCreateProfileSubPage);
  ProfileMetrics::LogProfileAddNewUser(type);
}

void AvatarMenuActionsDesktop::EditProfile(Profile* profile) {
  Browser* settings_browser = browser_;
  if (!settings_browser) {
    settings_browser = Browser::Create(Browser::CreateParams(profile, true));
  }
  // TODO(davidben): The manageProfile page only allows editting the profile
  // associated with the browser it is opened in. AvatarMenuActionsDesktop
  // should account for this when picking a browser to open in.
  chrome::ShowSettingsSubPage(settings_browser, chrome::kManageProfileSubPage);
}

bool AvatarMenuActionsDesktop::ShouldShowAddNewProfileLink() const {
  // |browser_| can be NULL in unit_tests.
  if (browser_ && browser_->profile()->IsSupervised())
    return false;
  return true;
}

bool AvatarMenuActionsDesktop::ShouldShowEditProfileLink() const {
  return true;
}

void AvatarMenuActionsDesktop::ActiveBrowserChanged(Browser* browser) {
  browser_ = browser;
}
