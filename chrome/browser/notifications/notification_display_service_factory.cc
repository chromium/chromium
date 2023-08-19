// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service_factory.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

// static
NotificationDisplayService* NotificationDisplayServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<NotificationDisplayService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

// static
NotificationDisplayServiceFactory*
NotificationDisplayServiceFactory::GetInstance() {
  static base::NoDestructor<NotificationDisplayServiceFactory> instance;
  return instance.get();
}

NotificationDisplayServiceFactory::NotificationDisplayServiceFactory()
    : ProfileKeyedServiceFactory(
          "NotificationDisplayService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

KeyedService* NotificationDisplayServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(peter): Register the notification handlers here.
  return new NotificationDisplayServiceImpl(
      Profile::FromBrowserContext(context));
}
