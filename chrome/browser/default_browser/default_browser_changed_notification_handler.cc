// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_changed_notification_handler.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"

namespace default_browser {

DefaultBrowserChangedNotificationHandler::
    DefaultBrowserChangedNotificationHandler() = default;

DefaultBrowserChangedNotificationHandler::
    ~DefaultBrowserChangedNotificationHandler() = default;

void DefaultBrowserChangedNotificationHandler::OnShow(
    Profile* profile,
    const std::string& notification_id) {
  if (notification_id != DefaultBrowserManager::kNotificationId) {
    return;
  }

  // Create a new controller for this interaction.
  controller_ =
      DefaultBrowserManager::From(g_browser_process)
          ->CreateControllerFor(
              DefaultBrowserEntrypointType::kChangeDetectedNotification);
  CHECK(controller_);
  controller_->OnShown();
}

void DefaultBrowserChangedNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  base::ScopedClosureRunner completion_runner(std::move(completed_closure));

  if (notification_id != DefaultBrowserManager::kNotificationId) {
    return;
  }

  if (auto controller = std::exchange(controller_, nullptr)) {
    // If the notification closes without a click action (e.g. user-initiated
    // close or system timeout), record the outcome as ignored or dismissed.
    if (by_user) {
      controller->OnDismissed();
    } else {
      controller->OnIgnored();
    }
  }
}

void DefaultBrowserChangedNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  base::ScopedClosureRunner completion_runner(std::move(completed_closure));

  if (notification_id != DefaultBrowserManager::kNotificationId) {
    return;
  }

  auto controller = std::exchange(controller_, nullptr);
  if (!controller) {
    return;
  }

  if (!action_index.has_value() || action_index.value() == 0) {
    auto* controller_ptr = controller.get();
    controller_ptr->OnAccepted(
        base::DoNothingWithBoundArgs(std::move(controller)));
  } else if (action_index.value() == 1) {
    controller->OnDismissed();
  } else {
    NOTREACHED();
  }

  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationHandler::Type::DEFAULT_BROWSER_CHANGED, notification_id);
}

}  // namespace default_browser
