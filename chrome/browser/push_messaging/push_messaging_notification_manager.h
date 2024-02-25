// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_NOTIFICATION_MANAGER_H_

#include <stdint.h>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/push_messaging/budget_database.h"
#include "extensions/buildflags/buildflags.h"

class GURL;
class Profile;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SilentPushEvent {
  kSilentRequest = 0,
  kNotificationEnforcementSkipped = 1,
  kAllowedWithoutNotification = 2,
  kAllowedWithGenericNotification = 3,
  kMaxValue = kAllowedWithGenericNotification,
};

namespace content {
class WebContents;
}  // namespace content

// Developers may be required to display a Web Notification in response to an
// incoming push message in order to clarify to the user that something has
// happened in the background. When they forget to do so, a default notification
// has to be displayed on their behalf.
//
// This class implements the heuristics for determining whether the default
// notification is necessary, as well as the functionality of displaying the
// default notification when it is.
//
// See the following document and bug for more context:
// https://docs.google.com/document/d/13VxFdLJbMwxHrvnpDm8RXnU41W2ZlcP0mdWWe9zXQT8/edit
// https://crbug.com/437277
class PushMessagingNotificationManager {
 public:
  using EnforceRequirementsCallback =
      base::OnceCallback<void(bool did_show_generic_notification)>;

  explicit PushMessagingNotificationManager(Profile* profile);

  PushMessagingNotificationManager(const PushMessagingNotificationManager&) =
      delete;
  PushMessagingNotificationManager& operator=(
      const PushMessagingNotificationManager&) = delete;

  ~PushMessagingNotificationManager();

  // Enforces the requirements implied for push subscriptions which must display
  // a Web Notification in response to an incoming message.
  // `requested_user_visible_only` is the userVisibleOnly value a worker based
  // extension sets on push subscription.
  void EnforceUserVisibleOnlyRequirements(
      const GURL& origin,
      int64_t service_worker_registration_id,
      EnforceRequirementsCallback message_handled_callback,
      bool requested_user_visible_only);

  // Checks if userVisibleOnly can be skipped in certain scenarios. Currently
  // that is only allowed for extensions that set userVisibleOnly as false on
  // subscription.
  bool ShouldSkipUserVisibleOnlyRequirements(const GURL& origin,
                                             bool requested_user_visible_only);

 private:
  FRIEND_TEST_ALL_PREFIXES(PushMessagingNotificationManagerTest, IsTabVisible);
  FRIEND_TEST_ALL_PREFIXES(PushMessagingNotificationManagerTest,
                           IsTabVisibleViewSource);

  void DidCountVisibleNotifications(
      const GURL& origin,
      int64_t service_worker_registration_id,
      EnforceRequirementsCallback message_handled_callback,
      bool success,
      int notification_count);

  // Checks whether |profile| is the one owning this instance,
  // |active_web_contents| exists and its main frame is visible, and the URL
  // currently visible to the user is for |origin|.
  bool IsTabVisible(Profile* profile,
                    content::WebContents* active_web_contents,
                    const GURL& origin);

  void ProcessSilentPush(const GURL& origin,
                         int64_t service_worker_registration_id,
                         EnforceRequirementsCallback message_handled_callback,
                         bool silent_push_allowed);

  void DidWriteNotificationData(
      EnforceRequirementsCallback message_handled_callback,
      bool success,
      const std::string& notification_id);

  void LogSilentPushEvent(SilentPushEvent event);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // For extensions builds, skip userVisibleOnly requirement for worker-based
  // extensions that set it to false.
  bool ShouldSkipExtensionUserVisibleOnlyRequirements(
      const GURL& origin,
      bool requested_user_visible_only);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Weak. This manager is owned by a keyed service on this profile.
  raw_ptr<Profile> profile_;

  BudgetDatabase budget_database_;

  base::WeakPtrFactory<PushMessagingNotificationManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_NOTIFICATION_MANAGER_H_
