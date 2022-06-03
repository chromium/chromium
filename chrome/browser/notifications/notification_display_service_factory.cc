// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service_factory.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
NotificationDisplayService* NotificationDisplayServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NotificationDisplayService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

// static
NotificationDisplayServiceFactory*
NotificationDisplayServiceFactory::GetInstance() {
  return base::Singleton<NotificationDisplayServiceFactory>::get();
}

NotificationDisplayServiceFactory::NotificationDisplayServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "NotificationDisplayService",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* NotificationDisplayServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(peter): Register the notification handlers here.
  return new NotificationDisplayServiceImpl(
      Profile::FromBrowserContext(context));
}

content::BrowserContext*
NotificationDisplayServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
