// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/badging/badge_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class Profile;

namespace badging {

// A BadgeManagerDelegate is responsible for updating the UI in response to a
// badge change.
class BadgeManagerDelegate {
 public:
  explicit BadgeManagerDelegate(Profile* profile, BadgeManager* badge_manager)
      : profile_(profile), badge_manager_(badge_manager) {}

  BadgeManagerDelegate(const BadgeManagerDelegate&) = delete;
  BadgeManagerDelegate& operator=(const BadgeManagerDelegate&) = delete;

  virtual ~BadgeManagerDelegate() = default;

  // Called when the badge for |app_id| has changed.
  virtual void OnAppBadgeUpdated(const webapps::AppId& app_id) = 0;

 protected:
  Profile* profile() { return profile_; }
  BadgeManager* badge_manager() { return badge_manager_; }

 private:
  // The profile the badge manager delegate is associated with.
  raw_ptr<Profile, DanglingUntriaged> profile_;
  // The badge manager that owns this delegate.
  raw_ptr<BadgeManager> badge_manager_;
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_H_
