// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_TEST_BADGE_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_BADGING_TEST_BADGE_MANAGER_DELEGATE_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace badging {

class BadgeManager;

using AppBadge = std::pair<webapps::AppId, BadgeManager::BadgeValue>;
using ScopeBadge = std::pair<GURL, BadgeManager::BadgeValue>;

// Testing delegate that records badge changes for apps.
class TestBadgeManagerDelegate : public BadgeManagerDelegate {
 public:
  TestBadgeManagerDelegate(Profile* profile, BadgeManager* badge_manager);
  ~TestBadgeManagerDelegate() override;

  // Sets a callback to fire when a badge is set or cleared.
  void SetOnBadgeChanged(base::RepeatingCallback<void()> on_badge_changed);

  // Resets the lists of cleared and set badges.
  void ResetBadges();

  std::vector<webapps::AppId> cleared_badges() { return cleared_badges_; }
  std::vector<AppBadge> set_badges() { return set_badges_; }

  // BadgeManagerDelegate:
  void OnAppBadgeUpdated(const webapps::AppId& app_badge) override;

 protected:

 private:
  base::RepeatingCallback<void()> on_badge_changed_;

  std::vector<webapps::AppId> cleared_badges_;
  std::vector<AppBadge> set_badges_;
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_TEST_BADGE_MANAGER_DELEGATE_H_
