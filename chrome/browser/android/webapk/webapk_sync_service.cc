// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_service.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"

namespace webapk {

// static
WebApkSyncService* WebApkSyncService::GetForProfile(Profile* profile) {
  return WebApkSyncServiceFactory::GetForProfile(profile);
}

WebApkSyncService::WebApkSyncService(Profile* profile) {
  database_factory_ = std::make_unique<WebApkDatabaseFactory>(profile);
  sync_bridge_ = std::make_unique<WebApkSyncBridge>(database_factory_.get(),
                                                    base::DoNothing());
}

WebApkSyncService::~WebApkSyncService() = default;

}  // namespace webapk
