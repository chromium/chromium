// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_FEATURES_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// Feature flag to disallow creation of push messages with GCM Sender IDs.
BASE_DECLARE_FEATURE(kPushMessagingDisallowSenderIDs);

// Feature flag to enable push subscription with expiration times specified in
// /chrome/browser/push_messaging/push_messaging_constants.h
BASE_DECLARE_FEATURE(kPushSubscriptionWithExpirationTime);

// Feature flag to control which environment |kPushMessagingGcmEndpoint|
// corresponds to.
BASE_DECLARE_FEATURE(kPushMessagingGcmEndpointEnvironment);

#if BUILDFLAG(IS_ANDROID)
// Feature flag to revoke site-level Notifications permissions and FCM
// registration.
BASE_DECLARE_FEATURE(kRevokeNotificationsPermissionIfDisabledOnAppLevel);

// Name of the variation parameter that represents the grace period that will be
// applied before site-level Notifications permissions will be revoked and FCM
// unsubscribed. The default value is 3.
extern const char kNotificationRevocationGracePeriodInDays[];
#endif

}  // namespace features

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_FEATURES_H_
