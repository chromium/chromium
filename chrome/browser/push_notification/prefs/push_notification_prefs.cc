// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/prefs/push_notification_prefs.h"

#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"

namespace push_notification {

namespace prefs {

const char kPushNotificationRegistrationAttemptBackoffSchedulerPrefName[] =
    "push_notification.scheduler.registration_attempt_backoff";

}  // namespace prefs

// TODO(b/306399332): Add pref for `representative_target_id` to be used when
// registering with Chime.
void RegisterPushNotificationPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kPushNotificationRegistrationAttemptBackoffSchedulerPrefName);
}

}  // namespace push_notification
