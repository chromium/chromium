// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_features.h"

namespace features {

const base::Feature kPushMessagingDisallowSenderIDs{
    "PushMessagingDisallowSenderIDs", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPushSubscriptionWithExpirationTime{
    "PushSubscriptionWithExpirationTime", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID)
const base::Feature kRevokeNotificationsPermissionIfDisabledOnAppLevel{
    "RevokeNotificationsPermissionIfDisabledOnAppLevel",
    base::FEATURE_DISABLED_BY_DEFAULT};

const char kNotificationRevocationGracePeriodInDays[] =
    "notifications_revocation_grace_period";
#endif

}  // namespace features
