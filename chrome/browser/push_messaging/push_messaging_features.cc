// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_features.h"

#include "chrome/browser/push_messaging/push_messaging_constants.h"

namespace features {

BASE_FEATURE(kPushMessagingDisallowSenderIDs,
             "PushMessagingDisallowSenderIDs",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPushSubscriptionWithExpirationTime,
             "PushSubscriptionWithExpirationTime",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPushMessagingGcmEndpointEnvironment,
             "PushMessagingGcmEndpointEnvironment",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kRevokeNotificationsPermissionIfDisabledOnAppLevel,
             "RevokeNotificationsPermissionIfDisabledOnAppLevel",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kNotificationRevocationGracePeriodInDays[] =
    "notifications_revocation_grace_period";
#endif

}  // namespace features
