// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_MAC_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_MAC_H_

#include "chrome/browser/badging/badge_manager_delegate.h"

class Profile;

namespace badging {

class BadgeManager;

// OSX specific implementation of the BadgeManagerDelegate.
class BadgeManagerDelegateMac : public BadgeManagerDelegate {
 public:
  explicit BadgeManagerDelegateMac(Profile* profile,
                                   BadgeManager* badge_manager);

  void OnAppBadgeUpdated(const webapps::AppId& app_id) override;
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_MAC_H_
