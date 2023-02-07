// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/browser_context_keyed_service_factories.h"

#include "build/chromeos_buildflags.h"

#include "chrome/browser/apps/platform_apps/api/media_galleries/media_galleries_api.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/extension_sync_event_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/platform_apps/api/arc_apps_private/arc_apps_private_api.h"
#endif

namespace chrome_apps::api {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ArcAppsPrivateAPI::GetFactoryInstance();
#endif
  ExtensionSyncEventObserver::GetFactoryInstance();
  MediaGalleriesEventRouter::GetFactoryInstance();
}

}  // namespace chrome_apps::api
