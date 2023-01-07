// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DISPLAYED_NOTIFICATIONS_DISPATCH_CALLBACK_H_
#define CHROME_BROWSER_NOTIFICATIONS_DISPLAYED_NOTIFICATIONS_DISPATCH_CALLBACK_H_

#include <set>
#include <string>

#include "base/functional/callback.h"

// Callback used by the bridge and all the downstream classes that propagate
// the callback to get displayed notifications.
//
// |supports_synchronization| will be true if the platform supports getting the
// currently displayed notifications.
//
// If |supports_synchronization| is true, then |notification_ids| will contain
// the ids of the currently displayed notifications, otherwise the value of
// |notification_ids| should be ignored.
using GetDisplayedNotificationsCallback =
    base::OnceCallback<void(std::set<std::string> notification_ids,
                            bool supports_synchronization)>;

#endif  // CHROME_BROWSER_NOTIFICATIONS_DISPLAYED_NOTIFICATIONS_DISPATCH_CALLBACK_H_
