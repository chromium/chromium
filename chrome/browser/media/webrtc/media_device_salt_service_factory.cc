// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"

#include "base/files/file_path.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

MediaDeviceSaltServiceFactory* MediaDeviceSaltServiceFactory::GetInstance() {
  static base::NoDestructor<MediaDeviceSaltServiceFactory> factory;
  return factory.get();
}

media_device_salt::MediaDeviceSaltService*
MediaDeviceSaltServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<media_device_salt::MediaDeviceSaltService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

MediaDeviceSaltServiceFactory::MediaDeviceSaltServiceFactory()
    : ProfileKeyedServiceFactory(
          "MediaDeviceSaltServiceFactory",
          ProfileSelections::Builder()
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

std::unique_ptr<KeyedService>
MediaDeviceSaltServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<media_device_salt::MediaDeviceSaltService>(
      user_prefs::UserPrefs::Get(context),
      context->IsOffTheRecord()
          ? base::FilePath()
          : context->GetPath().AppendASCII("MediaDeviceSalts"));
}
