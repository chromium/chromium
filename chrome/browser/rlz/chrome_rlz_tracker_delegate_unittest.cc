// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationEntry;
using content::LoadCommittedDetails;

class ChromeRLZTrackerDelegateTest : public testing::Test {
 public:
  ChromeRLZTrackerDelegateTest() : delegate_(new ChromeRLZTrackerDelegate) {}
  ~ChromeRLZTrackerDelegateTest() override {}

  void SendNotification(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
    content::NotificationObserver* observer = delegate_.get();
    observer->Observe(type, source, details);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ChromeRLZTrackerDelegate> delegate_;
};

TEST_F(ChromeRLZTrackerDelegateTest, ObserveHandlesBadArgs) {
  std::unique_ptr<content::LoadCommittedDetails> details(
      new content::LoadCommittedDetails());
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationEntry::Create());
  details->entry = entry.get();
  details->entry->SetTransitionType(ui::PAGE_TRANSITION_LINK);
  SendNotification(content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::NotificationService::AllSources(),
                   content::Details<NavigationEntry>(nullptr));
  SendNotification(content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::NotificationService::AllSources(),
                   content::Details<LoadCommittedDetails>(details.get()));
}
