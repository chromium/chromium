// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_WEBSITE_APPROVAL_NOTIFIER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_WEBSITE_APPROVAL_NOTIFIER_H_

#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace ash {

// Displays system notifications when new websites are remotely approved for
// this child account. This class listens to the SupervisedUserSettingsService
// for new remote approvals.
class WebsiteApprovalNotifier {
 public:
  explicit WebsiteApprovalNotifier(Profile* profile);

  WebsiteApprovalNotifier(const WebsiteApprovalNotifier&) = delete;
  WebsiteApprovalNotifier& operator=(const WebsiteApprovalNotifier&) = delete;

  ~WebsiteApprovalNotifier();

 private:
  friend class WebsiteApprovalNotifierTest;

  // |allowed_host| can be an exact hostname, or a pattern containing wildcards.
  // Refer to SupervisedUserURLFilter::HostMatchesPattern for details and
  // examples.
  // This method displays a system notification If |allowed_host| is an exact
  // hostname. Clicking the notification opens the site (defaulting to https) in
  // a new tab.
  // No notification is shown if |allowed_host| is a match pattern.
  void MaybeShowApprovalNotification(const std::string& allowed_host);

  const raw_ptr<Profile> profile_;

  base::CallbackListSubscription website_approval_subscription_;

  base::WeakPtrFactory<WebsiteApprovalNotifier> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_WEBSITE_APPROVAL_NOTIFIER_H_
