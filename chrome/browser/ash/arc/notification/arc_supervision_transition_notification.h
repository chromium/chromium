// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_SUPERVISION_TRANSITION_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_SUPERVISION_TRANSITION_NOTIFICATION_H_

class Profile;

namespace arc {

// To share with unit tests.
extern const char kSupervisionTransitionNotificationId[];

// Shows supervision transition notification that notifies that ARC++ is not
// reachable while supervision transition is in progress. This is informative
// notification only. This notification is automatically dismissed when ARC++ is
// opted out or supervision transition is completed.
void ShowSupervisionTransitionNotification(Profile* profile);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NOTIFICATION_ARC_SUPERVISION_TRANSITION_NOTIFICATION_H_
