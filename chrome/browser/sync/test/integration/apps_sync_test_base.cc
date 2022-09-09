// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/apps_sync_test_base.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/sync/chrome_sync_client.h"
#endif

AppsSyncTestBase::AppsSyncTestBase(TestType test_type) : SyncTest(test_type) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  browser_sync::ChromeSyncClient::SkipMainProfileCheckForTesting();
#endif
}

AppsSyncTestBase::~AppsSyncTestBase() = default;
