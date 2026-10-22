// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/file_system_backends.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

// A BrowserContext without a path, e.g. the Top Chrome WebUI profile, gets no
// additional file system backends, since they need the path for their files.
TEST(FileSystemBackendsTest, NoAdditionalFileSystemBackendsWithoutPath) {
  content::BrowserTaskEnvironment task_environment;
  webui::ScopedTopChromeWebUIProfileForTesting scoped_profile;
  Profile* top_chrome_webui_profile = webui::GetTopChromeWebUIProfile();
  ASSERT_TRUE(top_chrome_webui_profile->GetPath().empty());

  std::vector<std::unique_ptr<storage::FileSystemBackend>> backends;
  GetAdditionalFileSystemBackends(
      top_chrome_webui_profile, top_chrome_webui_profile->GetPath(), &backends);
  EXPECT_TRUE(backends.empty());
}
