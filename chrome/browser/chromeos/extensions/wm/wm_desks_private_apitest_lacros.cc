// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

namespace extensions {

namespace {
constexpr base::TimeDelta kWaitForAnimationTimeout = base::Seconds(20);
}

// TODO(aprilzhou): Add lacros test for switch desk once switching desk is
// implemented in lacros.
class DesksExtensionApiLacrosTest : public extensions::ExtensionApiTest {
 public:
  DesksExtensionApiLacrosTest() = default;
  ~DesksExtensionApiLacrosTest() override = default;
  void WaitForDeskAnimation(chromeos::LacrosService* lacros_service,
                            base::TimeDelta animation_timeout) {
    auto are_desks_being_modified = true;
    auto start = base::Time::NowFromSystemTime();
    while (are_desks_being_modified) {
      base::test::TestFuture<bool> future;
      lacros_service->GetRemote<crosapi::mojom::TestController>()
          ->AreDesksBeingModified(future.GetCallback());
      are_desks_being_modified = future.Get();
      auto now = base::Time::NowFromSystemTime();
      if (start + animation_timeout < now)
        FAIL() << "Desk animation timeout";
    }
  }
};

// Use API test for tests require other chrome API.
// API test is flaky when involves animation. For APIs involving animation use
// browser test instead.
IN_PROC_BROWSER_TEST_F(DesksExtensionApiLacrosTest,
                       DesksExtensionApiLacrosTest) {
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();

  if (!lacros_service->IsAvailable<crosapi::mojom::Desk>() ||
      lacros_service->GetInterfaceVersion(crosapi::mojom::Desk::Uuid_) <
          static_cast<int>(crosapi::mojom::Desk::MethodMinVersions::
                               kGetSavedDesksMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
  // This loads and runs an extension from
  // chrome/test/data/extensions/api_test/wm_desks_private.
  ASSERT_TRUE(RunExtensionTest("wm_desks_private")) << message_;
}

// Tests list all desks.
IN_PROC_BROWSER_TEST_F(DesksExtensionApiLacrosTest, ListDesksTest) {
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();

  if (!lacros_service->IsAvailable<crosapi::mojom::Desk>()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  auto list_desks_function =
      base::MakeRefCounted<WmDesksPrivateGetAllDesksFunction>();
  auto all_desks =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          list_desks_function.get(), "[]", browser());
  EXPECT_TRUE(all_desks->is_list());
}

// Tests launch and close a desk.
IN_PROC_BROWSER_TEST_F(DesksExtensionApiLacrosTest, LaunchAndCloseDeskTest) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Desk>()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();
  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto desk_id =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          launch_desk_function.get(), R"([{"deskName":"test"}])", browser());
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::GUID::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  // Wait for launch desk animation to settle.
  WaitForDeskAnimation(lacros_service, kWaitForAnimationTimeout);
  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 1, 1);

  // Remove a desk.
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  extension_function_test_utils::RunFunctionAndReturnSingleResult(
      remove_desk_function.get(),
      R"([")" + desk_id->GetString() + R"(", { "combineDesks": false }])",
      browser());

  // Wait for remove desk animation to settle.
  WaitForDeskAnimation(lacros_service, kWaitForAnimationTimeout);
  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 1, 1);
}

// Tests remove desks failed.
IN_PROC_BROWSER_TEST_F(DesksExtensionApiLacrosTest,
                       RemoveDeskWithInvalidIdTest) {
  auto* lacros_service = chromeos::LacrosService::Get();

  if (!lacros_service->IsAvailable<crosapi::mojom::Desk>()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }
  base::HistogramTester histogram_tester;
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  auto error = extension_function_test_utils::RunFunctionAndReturnError(
      remove_desk_function.get(), R"(["invalid-id"])", browser());

  EXPECT_EQ(error, "InvalidIdError");
  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 0, 1);
}

// Tests switch to different desk show trigger animation.
IN_PROC_BROWSER_TEST_F(DesksExtensionApiLacrosTest, SwitchToDifferentDeskTest) {
  auto* lacros_service = chromeos::LacrosService::Get();

  if (!lacros_service->IsAvailable<crosapi::mojom::Desk>() ||
      lacros_service->GetInterfaceVersion(crosapi::mojom::Desk::Uuid_) <
          static_cast<int>(
              crosapi::mojom::Desk::MethodMinVersions::kSwitchDeskMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  // Get the active desk.
  auto get_active_desk_function =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  // Asserts no error.
  auto desk_id =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_active_desk_function.get(), "[]", browser());
  ASSERT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::GUID::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  // Asserts no error.
  auto desk_id_1 =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          launch_desk_function.get(), R"([{"deskName":"test"}])", browser());
  ASSERT_TRUE(desk_id_1->is_string());
  EXPECT_TRUE(
      base::GUID::ParseCaseInsensitive(desk_id_1->GetString()).is_valid());
  // Waiting for desk launch animation to settle
  WaitForDeskAnimation(lacros_service, kWaitForAnimationTimeout);

  // Switches to the previous desk.
  auto switch_desk_function =
      base::MakeRefCounted<WmDesksPrivateSwitchDeskFunction>();

  extension_function_test_utils::RunFunctionAndReturnSingleResult(
      switch_desk_function.get(), R"([")" + desk_id->GetString() + R"("])",
      browser());

  WaitForDeskAnimation(lacros_service, kWaitForAnimationTimeout);

  auto get_active_desk_function_ =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  // Asserts no error.
  auto desk_id_2 =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_active_desk_function_.get(), "[]", browser());
  ASSERT_TRUE(desk_id_2->is_string());
  EXPECT_EQ(desk_id->GetString(), desk_id_2->GetString());

  // Clean up desks.
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  extension_function_test_utils::RunFunctionAndReturnSingleResult(
      remove_desk_function.get(),
      R"([")" + desk_id_1->GetString() + R"(", { "combineDesks": false }])",
      browser());

  // Wait for remove desk animation to settle.
  WaitForDeskAnimation(lacros_service, kWaitForAnimationTimeout);
}

// Tests switch to current desk should skip animation.
IN_PROC_BROWSER_TEST_F(DesksExtensionApiLacrosTest, SwitchToCurrentDeskTest) {
  auto* lacros_service = chromeos::LacrosService::Get();

  if (!lacros_service->IsAvailable<crosapi::mojom::Desk>() ||
      lacros_service->GetInterfaceVersion(crosapi::mojom::Desk::Uuid_) <
          static_cast<int>(
              crosapi::mojom::Desk::MethodMinVersions::kSwitchDeskMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  // Get the active desk.
  auto get_active_desk_function =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  // Asserts no error.
  auto desk_id =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_active_desk_function.get(), "[]", browser());
  ASSERT_TRUE(desk_id->is_string());
  ASSERT_TRUE(
      base::GUID::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  // Switches to the current desk.
  auto switch_desk_function =
      base::MakeRefCounted<WmDesksPrivateSwitchDeskFunction>();
  extension_function_test_utils::RunFunctionAndReturnSingleResult(
      switch_desk_function.get(), R"([")" + desk_id->GetString() + R"("])",
      browser());

  // Get the current desk.
  auto get_active_desk_function_ =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  auto desk_id_1 =
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_active_desk_function_.get(), "[]", browser());
  ASSERT_TRUE(desk_id_1->is_string());
  EXPECT_EQ(desk_id->GetString(), desk_id_1->GetString());
}

// TODO(b/254500921): Find a way to create new MRU window in ash-chrome and
// test save&recall in lacros.

}  // namespace extensions
