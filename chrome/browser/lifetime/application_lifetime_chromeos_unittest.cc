// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime_chromeos.h"

#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

TEST(ApplicationLifetimeChromeosTest, NoUpdateIfDbusIsNotInitialized) {
  ASSERT_FALSE(ash::DBusThreadManager::IsInitialized());
  EXPECT_FALSE(UpdatePending());
}

}  // namespace chrome
