// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service.h"

#include "chrome/browser/notifications/notification_display_service_factory.h"

NotificationDisplayService::Observer::~Observer() = default;

// static
NotificationDisplayService* NotificationDisplayService::GetForProfile(
    Profile* profile) {
  return NotificationDisplayServiceFactory::GetForProfile(profile);
}

NotificationDisplayService::~NotificationDisplayService() = default;
