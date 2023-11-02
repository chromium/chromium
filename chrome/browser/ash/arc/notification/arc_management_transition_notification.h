// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_MANAGEMENT_TRANSITION_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_MANAGEMENT_TRANSITION_NOTIFICATION_H_

class Profile;

namespace arc {

// To share with unit tests.
extern const char kManagementTransitionNotificationId[];

// Shows management transition notification that notifies that ARC is not
// reachable while management transition is in progress. This is informative
// notification only. This notification is automatically dismissed when ARC++ is
// opted out or management transition is completed.
void ShowManagementTransitionNotification(Profile* profile);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_MANAGEMENT_TRANSITION_NOTIFICATION_H_
