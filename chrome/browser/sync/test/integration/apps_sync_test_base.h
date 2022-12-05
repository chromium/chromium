// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_SYNC_TEST_BASE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_SYNC_TEST_BASE_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

class AppsSyncTestBase : public SyncTest {
 public:
  explicit AppsSyncTestBase(TestType test_type);
  ~AppsSyncTestBase() override;

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_SYNC_TEST_BASE_H_
