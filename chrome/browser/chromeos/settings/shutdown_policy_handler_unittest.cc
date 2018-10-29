// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/shutdown_policy_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ShutdownPolicyHandlerTest : public testing::Test,
                                  public ShutdownPolicyHandler::Delegate {
 public:
  ShutdownPolicyHandlerTest()
      : reboot_on_shutdown_(false), delegate_invocations_count_(0) {}

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    DBusThreadManager::Initialize();
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
  }

  void TearDown() override { DBusThreadManager::Shutdown(); }

  void SetRebootOnShutdown(bool reboot_on_shutdown) {
    settings_helper_.SetBoolean(kRebootOnShutdown, reboot_on_shutdown);
    base::RunLoop().RunUntilIdle();
  }

  // ShutdownPolicyHandler::Delegate:
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override {
    reboot_on_shutdown_ = reboot_on_shutdown;
    delegate_invocations_count_++;
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  ScopedCrosSettingsTestHelper settings_helper_;
  bool reboot_on_shutdown_;
  int delegate_invocations_count_;
};

TEST_F(ShutdownPolicyHandlerTest, RetrieveTrustedDevicePolicies) {
  ShutdownPolicyHandler shutdown_policy_handler(CrosSettings::Get(), this);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, delegate_invocations_count_);

  SetRebootOnShutdown(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate_invocations_count_);
  EXPECT_TRUE(reboot_on_shutdown_);

  SetRebootOnShutdown(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, delegate_invocations_count_);
  EXPECT_FALSE(reboot_on_shutdown_);
}

TEST_F(ShutdownPolicyHandlerTest, NotifyDelegateWithShutdownPolicy) {
  ShutdownPolicyHandler shutdown_policy_handler(CrosSettings::Get(), this);
  base::RunLoop().RunUntilIdle();

  // Allow shutdown.
  SetRebootOnShutdown(true);
  delegate_invocations_count_ = 0;
  // Request a manual update.
  shutdown_policy_handler.NotifyDelegateWithShutdownPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate_invocations_count_);
  EXPECT_TRUE(reboot_on_shutdown_);

  // Forbid shutdown.
  SetRebootOnShutdown(false);
  delegate_invocations_count_ = 0;
  // Request a manual update.
  shutdown_policy_handler.NotifyDelegateWithShutdownPolicy();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate_invocations_count_);
  EXPECT_FALSE(reboot_on_shutdown_);
}

}  // namespace chromeos
