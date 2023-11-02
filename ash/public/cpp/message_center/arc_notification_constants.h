// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATION_CONSTANTS_H_
#define ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATION_CONSTANTS_H_

namespace ash {

// The fallback notifier id for ARC notifications. Used when ArcNotificationItem
// is provided with an empty app id.
constexpr char kDefaultArcNotifierId[] = "ARC_NOTIFICATION";

// The prefix used when ARC generates a notification id, which is used in Chrome
// world, from a notification key, which is used in Android.
constexpr char kArcNotificationIdPrefix[] = "ARC_NOTIFICATION_";

// The custom view type that should be set on ARC notifications.
constexpr char kArcNotificationCustomViewType[] = "arc";

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MESSAGE_CENTER_ARC_NOTIFICATION_CONSTANTS_H_
