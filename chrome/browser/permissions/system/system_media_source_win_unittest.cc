// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_media_source_win.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

class SystemMediaSourceWinTest : public testing::Test {
 protected:
  SystemMediaSourceWin& CreateSource() {
    source_ = new SystemMediaSourceWin();
    return *source_;
  }

  void TearDown() override {
    SystemMediaSourceWin* raw = source_.ExtractAsDangling();
    delete raw;
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<SystemMediaSourceWin> source_ = nullptr;
};

// Verify that querying system permission status completes without crashing or
// pumping window messages. The COM message filter suppresses message
// dispatching during RoGetActivationFactory, preventing re-entrancy crashes.
TEST_F(SystemMediaSourceWinTest, ConstructionDoesNotCrash) {
  auto& source = CreateSource();

  // Each call queries the Windows AppCapability WinRT API directly (caching is
  // handled by SystemMediaPermissionCache). Transient WinRT failures on loaded
  // bots can return kNotDetermined on one call and a concrete status on
  // another, so only assert consistency when both calls return a determined
  // status.
  auto check_status = [&](ContentSettingsType type) {
    auto first = source.SystemPermissionStatus(type);
    auto second = source.SystemPermissionStatus(type);
    if (first != SystemMediaSourceWin::Status::kNotDetermined &&
        second != SystemMediaSourceWin::Status::kNotDetermined) {
      EXPECT_EQ(first, second);
    }
  };
  check_status(ContentSettingsType::MEDIASTREAM_CAMERA);
  check_status(ContentSettingsType::MEDIASTREAM_MIC);
}
