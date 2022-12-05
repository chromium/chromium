// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEB_APPS_SYNC_TEST_BASE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEB_APPS_SYNC_TEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

class WebAppsSyncTestBase : public SyncTest {
 public:
  explicit WebAppsSyncTestBase(TestType test_type);
  ~WebAppsSyncTestBase() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace web_app

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEB_APPS_SYNC_TEST_BASE_H_
