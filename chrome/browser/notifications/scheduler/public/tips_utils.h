// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_TIPS_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_TIPS_UTILS_H_

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

// Constructs and returns the NotificationData object for the requested feature.
// This mainly contains UI information and an enum representing the feature
// type. |feature_type| the feature in question to create a data object for.
NotificationData GetTipsNotificationData(
    TipsNotificationsFeatureType feature_type);

#if BUILDFLAG(IS_ANDROID)
// Returns the string representing the pref for recording whether the
// notification for the feature type in question has been shown before.
// |feature_type| the feature in question to return the pref for.
std::string GetFeatureTypePref(TipsNotificationsFeatureType feature_type);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_TIPS_UTILS_H_
