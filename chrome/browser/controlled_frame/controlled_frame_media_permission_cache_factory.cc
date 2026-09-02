// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_media_permission_cache_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/controlled_frame/controlled_frame_media_permission_cache.h"
#include "chrome/browser/profiles/profile.h"

namespace controlled_frame {

// static
ControlledFrameMediaPermissionCache*
ControlledFrameMediaPermissionCacheFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ControlledFrameMediaPermissionCache*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ControlledFrameMediaPermissionCacheFactory*
ControlledFrameMediaPermissionCacheFactory::GetInstance() {
  static base::NoDestructor<ControlledFrameMediaPermissionCacheFactory>
      instance;
  return instance.get();
}

ControlledFrameMediaPermissionCacheFactory::
    ControlledFrameMediaPermissionCacheFactory()
    : ProfileKeyedServiceFactory(
          "ControlledFrameMediaPermissionCache",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

ControlledFrameMediaPermissionCacheFactory::
    ~ControlledFrameMediaPermissionCacheFactory() = default;

std::unique_ptr<KeyedService> ControlledFrameMediaPermissionCacheFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<ControlledFrameMediaPermissionCache>(
      Profile::FromBrowserContext(context));
}

}  // namespace controlled_frame
