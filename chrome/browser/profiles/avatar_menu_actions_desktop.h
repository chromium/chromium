// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_AVATAR_MENU_ACTIONS_DESKTOP_H_
#define CHROME_BROWSER_PROFILES_AVATAR_MENU_ACTIONS_DESKTOP_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/profiles/avatar_menu_actions.h"
#include "chrome/browser/profiles/profile_metrics.h"

class Browser;
class Profile;

// Implements avatar menu actions for desktop, excluding Chrome OS.
class AvatarMenuActionsDesktop : public AvatarMenuActions {
 public:
  AvatarMenuActionsDesktop();
  ~AvatarMenuActionsDesktop() override;

  // AvatarMenuActions overrides:
  void AddNewProfile(ProfileMetrics::ProfileAdd type) override;
  void EditProfile(Profile* profile) override;
  bool ShouldShowAddNewProfileLink() const override;
  bool ShouldShowEditProfileLink() const override;
  void ActiveBrowserChanged(Browser* browser) override;

 private:
  // Browser in which this avatar menu resides. Weak.
  Browser* browser_;

  // Special "override" logout URL used to let tests work.
  std::string logout_override_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuActionsDesktop);
};

#endif  // CHROME_BROWSER_PROFILES_AVATAR_MENU_ACTIONS_DESKTOP_H_
