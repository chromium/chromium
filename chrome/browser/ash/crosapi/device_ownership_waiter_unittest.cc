// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_chromeos_version_info.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/device_ownership_waiter_impl.h"

#include <memory>

#include "base/check_deref.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

class DeviceOwnershipWaiterTest : public testing::Test {
 public:
  DeviceOwnershipWaiterTest() = default;

  ~DeviceOwnershipWaiterTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  ash::FakeChromeUserManager& GetFakeUserManager() {
    return CHECK_DEREF(static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get()));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ash::ScopedStubInstallAttributes> stub_install_attributes_{
      std::make_unique<ash::ScopedStubInstallAttributes>(
          ash::StubInstallAttributes::CreateUnset())};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
};

TEST_F(DeviceOwnershipWaiterTest, DelaysCorrectly) {
  // Since we skip the actual delay for ownership if we're running in a ChromeOS
  // on Linux build, we mock that behavior by pretending to be ChromeOS.
  const char kLsbReleaseValidChromeOs[] = "CHROMEOS_RELEASE_NAME=Chrome OS\n";

  {
    base::test::ScopedChromeOSVersionInfo version(kLsbReleaseValidChromeOs,
                                                  base::Time());
    DeviceOwnershipWaiterImpl waiter;

    base::test::TestFuture<void> future;
    waiter.WaitForOwnershipFetched(future.GetCallback(),
                                   /*launching_at_login_screen=*/true);

    GetFakeUserManager().SetOwnerId(user_manager::StubAccountId());

    EXPECT_TRUE(future.Wait());
  }
  {
    base::test::ScopedChromeOSVersionInfo version(kLsbReleaseValidChromeOs,
                                                  base::Time());
    DeviceOwnershipWaiterImpl waiter;

    base::test::TestFuture<void> future;
    waiter.WaitForOwnershipFetched(future.GetCallback(),
                                   /*launching_at_login_screen=*/false);

    GetFakeUserManager().SetOwnerId(user_manager::StubAccountId());

    EXPECT_TRUE(future.Wait());
  }
}

// Tests that on a ChromeOS on Linux build, the delay
// is skipped and instead the callback invoked immediately.
TEST_F(DeviceOwnershipWaiterTest, DoesNotDelayForChromeOsOnLinux) {
  const char kLsbReleaseNonChromeOs[] = "CHROMEOS_RELEASE_NAME=Non Chrome OS\n";
  base::test::ScopedChromeOSVersionInfo version(kLsbReleaseNonChromeOs,
                                                base::Time());

  DeviceOwnershipWaiterImpl waiter;

  base::test::TestFuture<void> future;
  waiter.WaitForOwnershipFetched(future.GetCallback(),
                                 /*launching_at_login_screen=*/false);

  EXPECT_TRUE(future.Wait());
}

}  // namespace crosapi
