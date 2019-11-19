// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/chrome_client_connection_manager.h"

#include "chrome/test/base/testing_profile.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace heap_profiling {

TEST(ChromeClientConnectionManager, ShouldProfileNewRenderer) {
  content::BrowserTaskEnvironment task_environment;

  ChromeClientConnectionManager manager(nullptr, Mode::kNone);

  TestingProfile testing_profile;
  content::MockRenderProcessHost rph(&testing_profile);

  manager.SetModeForTesting(Mode::kNone);
  EXPECT_FALSE(manager.ShouldProfileNewRenderer(&rph));

  manager.SetModeForTesting(Mode::kAll);
  EXPECT_TRUE(manager.ShouldProfileNewRenderer(&rph));

  Profile* incognito_profile = testing_profile.GetOffTheRecordProfile();
  content::MockRenderProcessHost incognito_rph(incognito_profile);
  EXPECT_FALSE(manager.ShouldProfileNewRenderer(&incognito_rph));
}

}  // namespace heap_profiling
