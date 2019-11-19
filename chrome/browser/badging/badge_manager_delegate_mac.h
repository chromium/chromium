// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_MAC_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_MAC_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/badging/badge_manager_delegate.h"

class Profile;

namespace badging {

class BadgeManager;

// OSX specific implementation of the BadgeManagerDelegate.
class BadgeManagerDelegateMac : public BadgeManagerDelegate {
 public:
  explicit BadgeManagerDelegateMac(Profile* profile,
                                   BadgeManager* badge_manager);

  void OnAppBadgeUpdated(const web_app::AppId& app_id) override;

 private:
  void SetAppBadgeLabel(const std::string& app_id,
                        const std::string& badge_label);
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_MAC_H_
