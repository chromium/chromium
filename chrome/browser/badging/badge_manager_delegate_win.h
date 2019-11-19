// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_WIN_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_WIN_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/ui/browser.h"

class Profile;

namespace badging {

class BadgeManager;

// Windows specific implementation of the BadgeManagerDelegate.
class BadgeManagerDelegateWin : public BadgeManagerDelegate {
 public:
  explicit BadgeManagerDelegateWin(Profile* profile,
                                   BadgeManager* badge_manager);

  void OnAppBadgeUpdated(const web_app::AppId& app_id) override;

 private:
  // Determines if a browser is for a specific hosted app, on this profile.
  bool IsAppBrowser(Browser* browser, const std::string& app_id);
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_WIN_H_
