// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/shutdown_policy_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ShutdownPolicyHandlerTest : public testing::Test,
                                  public ShutdownPolicyHandler::Delegate {
 public:
  ShutdownPolicyHandlerTest()
      : reboot_on_shutdown_(false), delegate_invocations_count_(0) {}

  void SetRebootOnShutdown(bool reboot_on_shutdown) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kRebootOnShutdown, reboot_on_shutdown);
    base::RunLoop().RunUntilIdle();
  }

  // ShutdownPolicyHandler::Delegate:
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override {
    reboot_on_shutdown_ = reboot_on_shutdown;
    delegate_invocations_count_++;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
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

}  // namespace ash
