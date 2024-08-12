// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/locale/startup_settings_cache.h"

#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class StartupSettingsCacheTest : public testing::Test {
 public:
  StartupSettingsCacheTest(const StartupSettingsCacheTest&) = delete;
  StartupSettingsCacheTest& operator=(const StartupSettingsCacheTest&) = delete;

 protected:
  StartupSettingsCacheTest() : user_data_dir_override_(chrome::DIR_USER_DATA) {}
  ~StartupSettingsCacheTest() override {}

 private:
  // Map DIR_USER_DATA to a temp dir.
  base::ScopedPathOverride user_data_dir_override_;
};

TEST_F(StartupSettingsCacheTest, RoundTrip) {
  startup_settings_cache::WriteAppLocale("foo");
  EXPECT_EQ("foo", startup_settings_cache::ReadAppLocale());
}

}  // namespace ash
