// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_notification_handler.h"

#include <utility>

#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "components/sharing_message/sharing_service.h"

SharingNotificationHandler::SharingNotificationHandler() = default;
SharingNotificationHandler::~SharingNotificationHandler() = default;

void SharingNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  auto handler = SharingServiceFactory::GetForBrowserContext(profile)
                     ->GetNotificationActionHandler(notification_id);
  if (handler) {
    handler.Run(action_index, /*closed=*/false);
  } else {
    // Close the notification by default.
    NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
        NotificationHandler::Type::SHARING, notification_id);
  }
  std::move(completed_closure).Run();
}

void SharingNotificationHandler::OnClose(Profile* profile,
                                         const GURL& origin,
                                         const std::string& notification_id,
                                         bool by_user,
                                         base::OnceClosure completed_closure) {
  auto handler = SharingServiceFactory::GetForBrowserContext(profile)
                     ->GetNotificationActionHandler(notification_id);
  if (handler)
    handler.Run(/*button=*/std::nullopt, /*closed=*/true);
  std::move(completed_closure).Run();
}

void SharingNotificationHandler::OpenSettings(Profile* profile,
                                              const GURL& origin) {}
