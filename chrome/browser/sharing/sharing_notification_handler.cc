// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_notification_handler.h"

#include <utility>

#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"

SharingNotificationHandler::SharingNotificationHandler() = default;
SharingNotificationHandler::~SharingNotificationHandler() = default;

void SharingNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    base::OnceClosure completed_closure) {
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationHandler::Type::SHARING, notification_id);
  std::move(completed_closure).Run();
}

void SharingNotificationHandler::OpenSettings(Profile* profile,
                                              const GURL& origin) {}
