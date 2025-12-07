// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/browser_context_keyed_service_factories.h"

#include "chrome/browser/apps/platform_apps/api/media_galleries/media_galleries_api.h"
#include "chrome/browser/apps/platform_apps/api/arc_apps_private/arc_apps_private_api.h"

namespace chrome_apps::api {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  ArcAppsPrivateAPI::GetFactoryInstance();
  MediaGalleriesEventRouter::GetFactoryInstance();
}

}  // namespace chrome_apps::api
