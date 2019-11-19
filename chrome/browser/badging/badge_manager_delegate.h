// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_H_

#include "base/optional.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "url/gurl.h"

class Profile;

namespace badging {

// A BadgeManagerDelegate is responsible for updating the UI in response to a
// badge change.
class BadgeManagerDelegate {
 public:
  explicit BadgeManagerDelegate(Profile* profile, BadgeManager* badge_manager)
      : profile_(profile), badge_manager_(badge_manager) {}

  virtual ~BadgeManagerDelegate() = default;

  // Called when the badge for |app_id| has changed.
  virtual void OnAppBadgeUpdated(const web_app::AppId& app_id) = 0;

 protected:
  Profile* profile() { return profile_; }
  BadgeManager* badge_manager() { return badge_manager_; }

 private:
  // The profile the badge manager delegate is associated with.
  Profile* profile_;
  // The badge manager that owns this delegate.
  BadgeManager* badge_manager_;

  DISALLOW_COPY_AND_ASSIGN(BadgeManagerDelegate);
};

}  // namespace badging

#endif  // CHROME_BROWSER_BADGING_BADGE_MANAGER_DELEGATE_H_
