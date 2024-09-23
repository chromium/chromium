// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

namespace {

bool IsSupportedAsh() {
  return (chromeos::LacrosService::Get()
              ->GetInterfaceVersion<crosapi::mojom::TestController>() >=
          static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                               kEnableStatisticsProviderForTestingMinVersion));
}

}  // namespace

// Tests chromeosInfoPrivate.get function for the properties provided by
// chromeos machine statistics.
class ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest
    : public extensions::ExtensionApiTest {
 public:
  ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest() = default;

  ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest(
      const ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest&) =
      delete;
  ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest& operator=(
      const ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest&) =
      delete;
  ~ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest() override =
      default;

  void SetUpOnMainThread() override {
    if (!IsSupportedAsh()) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }

    auto& test_controller = chromeos::LacrosService::Get()
                                ->GetRemote<crosapi::mojom::TestController>();
    base::test::TestFuture<void> future;
    test_controller->EnableStatisticsProviderForTesting(/*enable=*/true,
                                                        future.GetCallback());
    EXPECT_TRUE(future.Wait());

    extensions::ExtensionApiTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    if (!IsSupportedAsh()) {
      return;
    }

    auto& test_controller = chromeos::LacrosService::Get()
                                ->GetRemote<crosapi::mojom::TestController>();
    base::test::TestFuture<void> future;
    test_controller->ClearAllMachineStatistics(future.GetCallback());
    EXPECT_TRUE(future.WaitAndClear());
    test_controller->EnableStatisticsProviderForTesting(/*enable=*/false,
                                                        future.GetCallback());
    EXPECT_TRUE(future.Wait());

    extensions::ExtensionApiTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(
    ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest,
    TestPropertiesUnset) {
  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  base::test::TestFuture<void> future;
  test_controller->ClearAllMachineStatistics(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  ASSERT_TRUE(
      RunExtensionTest("chromeos_info_private/extended",
                       {.custom_arg = "Machine Statistics Properties - Unset",
                        .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(
    ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest,
    TestDeviceRequisitionRemora) {
  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  base::test::TestFuture<bool> future;
  test_controller->SetMachineStatistic(
      crosapi::mojom::MachineStatisticKeyType::kOemDeviceRequisitionKey,
      "remora", future.GetCallback());
  ASSERT_TRUE(future.Take());

  ASSERT_TRUE(RunExtensionTest("chromeos_info_private/extended",
                               {.custom_arg = "Device Requisition - Remora",
                                .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(
    ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest,
    TestHWID) {
  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  base::test::TestFuture<bool> future;
  test_controller->SetMachineStatistic(
      crosapi::mojom::MachineStatisticKeyType::kHardwareClassKey, "test_hw",
      future.GetCallback());
  ASSERT_TRUE(future.Take());

  ASSERT_TRUE(
      RunExtensionTest("chromeos_info_private/extended",
                       {.custom_arg = "HWID", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(
    ChromeOSInfoPrivateMachineStatisticsPropertyGetFunctionTest,
    TestCustomizationID) {
  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  base::test::TestFuture<bool> future;
  test_controller->SetMachineStatistic(
      crosapi::mojom::MachineStatisticKeyType::kCustomizationIdKey,
      "test_customization_id", future.GetCallback());
  ASSERT_TRUE(future.Take());

  ASSERT_TRUE(RunExtensionTest(
      "chromeos_info_private/extended",
      {.custom_arg = "CustomizationId", .launch_as_platform_app = true}))
      << message_;
}
