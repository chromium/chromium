// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/common/chrome_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#endif

namespace web_app {

WebAppsSyncTestBase::WebAppsSyncTestBase(TestType test_type)
    : SyncTest(test_type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Disable WebAppsCrosapi, so that Web Apps get synced in the Ash browser.
  scoped_feature_list_.InitWithFeatures(
      {}, {features::kWebAppsCrosapi, chromeos::features::kLacrosPrimary});
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SkipMainProfileCheckForTesting();
  browser_sync::ChromeSyncClient::SkipMainProfileCheckForTesting();
#endif
}

WebAppsSyncTestBase::~WebAppsSyncTestBase() = default;

}  // namespace web_app
