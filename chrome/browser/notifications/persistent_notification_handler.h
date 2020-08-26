// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/common/buildflags.h"

class ScopedKeepAlive;

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
               const base::Optional<int>& action_index,
               const base::Optional<base::string16>& reply,
               base::OnceClosure completed_closure) override;
  void DisableNotifications(Profile* profile, const GURL& origin) override;
  void OpenSettings(Profile* profile, const GURL& origin) override;

 private:
  void OnCloseCompleted(base::OnceClosure completed_closure,
                        content::PersistentNotificationStatus status);
  void OnClickCompleted(Profile* profile,
                        const std::string& notification_id,
                        base::OnceClosure completed_closure,
                        content::PersistentNotificationStatus status);

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  // Makes sure we keep the browser alive while the event in being processed.
  // As we have no control on the click handling, the notification could be
  // closed before a browser is brought up, thus terminating Chrome if it was
  // the last KeepAlive. (see https://crbug.com/612815)
  std::unique_ptr<ScopedKeepAlive> click_dispatch_keep_alive_;

  // Number of in-flight notification click events.
  int pending_click_dispatch_events_ = 0;
#endif

  base::WeakPtrFactory<PersistentNotificationHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_HANDLER_H_
