// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_HANDLER_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/common/buildflags.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace content {
enum class PersistentNotificationStatus;
}  // namespace content

// NotificationHandler implementation for persistent Web Notifications, that is,
// notifications associated with a Service Worker. Lives on the UI thread.
class PersistentNotificationHandler : public NotificationHandler {
 public:
  PersistentNotificationHandler();
  PersistentNotificationHandler(const PersistentNotificationHandler&) = delete;
  PersistentNotificationHandler& operator=(
      const PersistentNotificationHandler&) = delete;
  ~PersistentNotificationHandler() override;

  // NotificationHandler implementation.
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const std::optional<int>& action_index,
               const std::optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;
  void DisableNotifications(Profile* profile, const GURL& origin) override;
  void OpenSettings(Profile* profile, const GURL& origin) override;

 private:
  void OnCloseCompleted(Profile* profile,
                        base::OnceClosure completed_closure,
                        content::PersistentNotificationStatus status);
  void OnClickCompleted(Profile* profile,
                        const std::string& notification_id,
                        base::OnceClosure completed_closure,
                        content::PersistentNotificationStatus status);

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  class NotificationKeepAliveState {
   public:
    NotificationKeepAliveState(
        KeepAliveOrigin keep_alive_origin,
        ProfileKeepAliveOrigin profile_keep_alive_origin);
    ~NotificationKeepAliveState();

    void AddKeepAlive(Profile* profile);
    void RemoveKeepAlive(Profile* profile);

   private:
    const KeepAliveOrigin keep_alive_origin_;
    const ProfileKeepAliveOrigin profile_keep_alive_origin_;

    // Makes sure we keep the browser alive while the event in being processed.
    // As we have no control on the click handling, the notification could be
    // closed before a browser is brought up, thus terminating Chrome if it was
    // the last KeepAlive (see crbug.com/612815). We also need to wait until
    // close events got handled as we need to access the profile when removing
    // notifications from the NotificationDatabase (see crbug.com/1221601).
    std::unique_ptr<ScopedKeepAlive> event_dispatch_keep_alive_;

    // Same as |event_dispatch_keep_alive_|, but prevent Profile* deletion
    // instead of BrowserProcess teardown.
    std::map<Profile*, std::unique_ptr<ScopedProfileKeepAlive>>
        event_dispatch_profile_keep_alives_;

    // Number of in-flight notification events.
    int pending_dispatch_events_ = 0;

    // Number of in-flight notification events per Profile, for
    // |event_dispatch_profile_keep_alives_|.
    std::map<Profile*, int> profile_pending_dispatch_events_;
  };

  NotificationKeepAliveState click_event_keep_alive_state_{
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT,
      ProfileKeepAliveOrigin::kPendingNotificationClickEvent};
  NotificationKeepAliveState close_event_keep_alive_state_{
      KeepAliveOrigin::PENDING_NOTIFICATION_CLOSE_EVENT,
      ProfileKeepAliveOrigin::kPendingNotificationCloseEvent};
#endif

  base::WeakPtrFactory<PersistentNotificationHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_HANDLER_H_
