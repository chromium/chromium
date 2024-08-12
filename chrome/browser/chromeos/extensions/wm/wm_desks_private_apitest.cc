// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/spin_wait.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/extensions/wm/wm_desks_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"

namespace extensions {

class WmDesksPrivateApiTest : public ExtensionApiTest {
 public:
  WmDesksPrivateApiTest() {
    scoped_feature_list.InitWithFeatures(
        /*enabled_features=*/{ash::features::kDesksTemplates},
        /*disabled_features=*/{ash::features::kDeskTemplateSync});
  }

  ~WmDesksPrivateApiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

// Use API test for tests require other chrome API.
// API test is flaky when involves animation. For APIs involving animation use
// browser test instead.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, WmDesksPrivateApiTest) {
  // This loads and runs an extension from
  // chrome/test/data/extensions/api_test/wm_desks_private.
  ASSERT_TRUE(RunExtensionTest("wm_desks_private")) << message_;
}

// Tests launch and close a desk.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, LaunchAndCloseDeskTest) {
  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  ash::DeskSwitchAnimationWaiter launch_waiter;
  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto desk_id = api_test_utils::RunFunctionAndReturnSingleResult(
      launch_desk_function.get(), R"([{"deskName":"test"}])",
      browser()->profile());
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 1, 1);
  // Waiting for desk launch animation to settle
  // The check is necessary as both desk animation and extension function is
  // async. There is no guarantee which ones execute first.
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    launch_waiter.Wait();
  }

  ash::DeskSwitchAnimationWaiter remove_waiter;
  // Remove a desk.
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  api_test_utils::RunFunctionAndReturnSingleResult(
      remove_desk_function.get(),
      R"([")" + desk_id->GetString() +
          R"(", { "combineDesks": false, "allowUndo":false }])",
      browser()->profile());

  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 1, 1);
  // Waiting for desk removal animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    remove_waiter.Wait();
  }
  histogram_tester.ExpectUniqueSample("Ash.DeskApi.RemoveDeskType",
                                      ash::DeskCloseType::kCloseAllWindows, 1);
}

// Tests launch and removal of a desk. Makes sure desk cannot be undone after
// time has passed.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, LaunchAndAttemptUndo) {
  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  ash::DeskSwitchAnimationWaiter launch_waiter;
  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto desk_id = api_test_utils::RunFunctionAndReturnSingleResult(
      launch_desk_function.get(), R"([{"deskName":"test"}])",
      browser()->profile());
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 1, 1);
  // Waiting for desk launch animation to settle
  // The check is necessary as both desk animation and extension function is
  // async. There is no guarantee which ones execute first.
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    launch_waiter.Wait();
  }

  ash::DeskSwitchAnimationWaiter remove_waiter;
  // Remove a desk.
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  api_test_utils::RunFunctionAndReturnSingleResult(
      remove_desk_function.get(),
      R"([")" + desk_id->GetString() +
          R"(", { "combineDesks": false, "allowUndo": true }])",
      browser()->profile());

  //   Waiting for desk removal animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    remove_waiter.Wait();
  }

  EXPECT_TRUE(ash::DesksTestApi::DesksControllerCanUndoDeskRemoval());

  // Checks for if there are any other toasts running besides
  // the undo toast. Waits for other toasts to expire.
  if (!ash::ToastManager::Get()->IsToastShown("UndoCloseAllToast_1")) {
    LOG(INFO) << "Non-undo toast running, must wait for other toasts :(";
    SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(
        base::Seconds(45),
        ash::ToastManager::Get()->IsToastShown("UndoCloseAllToast_1"));
  }

  ash::WaitForMilliseconds(
      ash::ToastData::kDefaultToastDuration.InMilliseconds() +
      ash::DesksTestApi::GetCloseAllWindowCloseTimeout().InMilliseconds());

  EXPECT_FALSE(ash::DesksTestApi::DesksControllerCanUndoDeskRemoval());
  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 1, 1);
}

// TODO(crbug.com/40927214): Re-enable test that flakily fails
#if defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_LaunchAndUndo DISABLED_LaunchAndUndo
#else
#define MAYBE_LaunchAndUndo LaunchAndUndo
#endif
// Tests launch and removal of a desk. Tries to undo the removal.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, MAYBE_LaunchAndUndo) {
  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  ash::DeskSwitchAnimationWaiter launch_waiter;
  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto desk_id = api_test_utils::RunFunctionAndReturnSingleResult(
      launch_desk_function.get(), R"([{"deskName":"test"}])",
      browser()->profile());
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 1, 1);
  // Waiting for desk launch animation to settle
  // The check is necessary as both desk animation and extension function is
  // async. There is no guarantee which ones execute first.
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    launch_waiter.Wait();
  }

  ash::DeskSwitchAnimationWaiter remove_waiter;
  // Remove a desk.
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  api_test_utils::RunFunctionAndReturnSingleResult(
      remove_desk_function.get(),
      R"([")" + desk_id->GetString() +
          R"(", { "combineDesks": false, "allowUndo": true }])",
      browser()->profile());

  //   Waiting for desk removal animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    remove_waiter.Wait();
  }

  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Ash.DeskApi.RemoveDeskType", ash::DeskCloseType::kCloseAllWindowsAndWait,
      1);
  EXPECT_TRUE(ash::DesksTestApi::DesksControllerCanUndoDeskRemoval());

  ash::DesksController::Get()->MaybeCancelDeskRemoval();
  histogram_tester.ExpectTotalCount("Ash.DeskApi.CloseAllUndo", 1);
  EXPECT_FALSE(ash::DesksTestApi::DesksControllerCanUndoDeskRemoval());
  EXPECT_EQ(2, ash::DesksController::Get()->GetNumberOfDesks());
}

// Tests launch and removal of a desk. Should combine desks if both allowUndo
// and combineDesks are true.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, LaunchAndCombineUndoTrue) {
  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  ash::DeskSwitchAnimationWaiter launch_waiter;
  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto desk_id = api_test_utils::RunFunctionAndReturnSingleResult(
      launch_desk_function.get(), R"([{"deskName":"test"}])",
      browser()->profile());
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 1, 1);
  // Waiting for desk launch animation to settle
  // The check is necessary as both desk animation and extension function is
  // async. There is no guarantee which ones execute first.
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    launch_waiter.Wait();
  }

  ash::DeskSwitchAnimationWaiter remove_waiter;
  // Remove a desk.
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  api_test_utils::RunFunctionAndReturnSingleResult(
      remove_desk_function.get(),
      R"([")" + desk_id->GetString() +
          R"(", { "combineDesks": true, "allowUndo": true }])",
      browser()->profile());

  //   Waiting for desk removal animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    remove_waiter.Wait();
  }

  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 1, 1);
  histogram_tester.ExpectUniqueSample("Ash.DeskApi.RemoveDeskType",
                                      ash::DeskCloseType::kCombineDesks, 1);
  EXPECT_FALSE(ash::DesksTestApi::DesksControllerCanUndoDeskRemoval());
  EXPECT_EQ(1, ash::DesksController::Get()->GetNumberOfDesks());
}

// Tests launch and removal of a desk. Desk removal results in desks combining.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, LaunchAndRemoveCombine) {
  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  ash::DeskSwitchAnimationWaiter launch_waiter;
  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto desk_id = api_test_utils::RunFunctionAndReturnSingleResult(
      launch_desk_function.get(), R"([{"deskName":"test"}])",
      browser()->profile());
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 1, 1);
  // Waiting for desk launch animation to settle
  // The check is necessary as both desk animation and extension function is
  // async. There is no guarantee which ones execute first.
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    launch_waiter.Wait();
  }

  ash::DeskSwitchAnimationWaiter remove_waiter;
  // Remove a desk.
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  api_test_utils::RunFunctionAndReturnSingleResult(
      remove_desk_function.get(),
      R"([")" + desk_id->GetString() +
          R"(", { "combineDesks": true, "allowUndo": false }])",
      browser()->profile());

  //   Waiting for desk removal animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    remove_waiter.Wait();
  }

  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 1, 1);
  histogram_tester.ExpectUniqueSample("Ash.DeskApi.RemoveDeskType",
                                      ash::DeskCloseType::kCombineDesks, 1);

  EXPECT_EQ(1, ash::DesksController::Get()->GetNumberOfDesks());
}

// Tests launch and list all desk.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, ListDesksTest) {
  ash::DeskSwitchAnimationWaiter waiter;
  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  // Asserts no error.
  auto desk_id = api_test_utils::RunFunctionAndReturnSingleResult(
      launch_desk_function.get(), R"([{"deskName":"test"}])",
      browser()->profile());
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    waiter.Wait();
  }

  // List All Desks.
  auto list_desks_function =
      base::MakeRefCounted<WmDesksPrivateGetAllDesksFunction>();
  auto all_desks = api_test_utils::RunFunctionAndReturnSingleResult(
      list_desks_function.get(), "[]", browser()->profile());
  EXPECT_TRUE(all_desks->is_list());
  EXPECT_EQ(2u, all_desks->GetList().size());
}

// Tests switch to different desk show trigger animation.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, SwitchToDifferentDeskTest) {
  base::HistogramTester histogram_tester;
  // Get the active desk.
  auto get_active_desk_function =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  // Asserts no error.
  auto desk_id = api_test_utils::RunFunctionAndReturnSingleResult(
      get_active_desk_function.get(), "[]", browser()->profile());
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  ash::DeskSwitchAnimationWaiter launch_waiter;
  // Asserts no error.
  auto desk_id_1 = api_test_utils::RunFunctionAndReturnSingleResult(
      launch_desk_function.get(), R"([{"deskName":"test"}])",
      browser()->profile());
  EXPECT_TRUE(desk_id_1->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id_1->GetString()).is_valid());
  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 1, 1);

  // Waiting for desk launch animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    launch_waiter.Wait();
  }

  ash::DeskSwitchAnimationWaiter switch_waiter;
  // Switches to the previous desk.
  auto switch_desk_function =
      base::MakeRefCounted<WmDesksPrivateSwitchDeskFunction>();

  api_test_utils::RunFunctionAndReturnSingleResult(
      switch_desk_function.get(), R"([")" + desk_id->GetString() + R"("])",
      browser()->profile());

  // Waiting for desk launch animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    switch_waiter.Wait();
  }

  auto get_active_desk_function_ =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  // Asserts no error.
  auto desk_id_2 = api_test_utils::RunFunctionAndReturnSingleResult(
      get_active_desk_function_.get(), "[]", browser()->profile());
  EXPECT_TRUE(desk_id_2->is_string());
  EXPECT_EQ(desk_id->GetString(), desk_id_2->GetString());
  histogram_tester.ExpectBucketCount("Ash.DeskApi.SwitchDesk.Result", 1, 1);
}

// Tests switch to current desk should skip animation.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, SwitchToCurrentDeskTest) {
  // Get the desk desk.
  auto get_active_desk_function =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  // Asserts no error.
  auto desk_id = api_test_utils::RunFunctionAndReturnSingleResult(
      get_active_desk_function.get(), "[]", browser()->profile());
  EXPECT_TRUE(desk_id->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id->GetString()).is_valid());

  // Switches to the current desk.
  auto switch_desk_function =
      base::MakeRefCounted<WmDesksPrivateSwitchDeskFunction>();
  api_test_utils::RunFunctionAndReturnSingleResult(
      switch_desk_function.get(), R"([")" + desk_id->GetString() + R"("])",
      browser()->profile());

  // Get the current desk.
  auto get_active_desk_function_ =
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>();
  auto desk_id_1 = api_test_utils::RunFunctionAndReturnSingleResult(
      get_active_desk_function_.get(), "[]", browser()->profile());
  EXPECT_TRUE(desk_id_1->is_string());
  EXPECT_EQ(desk_id->GetString(), desk_id_1->GetString());
}

// Tests launch desks failed.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest,
                       LaunchDeskWhenMaxNumberExceedTest) {
  // Max number of desks allowed is 8.
  const int kMaxDeskIndex = 7;
  for (int i = 0; i < kMaxDeskIndex; i++) {
    ash::DesksController::Get()->NewDesk(ash::DesksCreationRemovalSource::kApi);
  }

  // Launch a desk.
  auto launch_desk_function =
      base::MakeRefCounted<WmDesksPrivateLaunchDeskFunction>();

  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto error = api_test_utils::RunFunctionAndReturnError(
      launch_desk_function.get(), R"([{"deskName":"test"}])",
      browser()->profile());
  EXPECT_EQ(error, "DesksCountCheckFailedError");
  histogram_tester.ExpectBucketCount("Ash.DeskApi.LaunchDesk.Result", 0, 1);
}

// Tests remove desks failed.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, RemoveDeskWithInvalidIdTest) {
  base::HistogramTester histogram_tester;
  // The RunFunctionAndReturnSingleResult already asserts no error
  auto remove_desk_function =
      base::MakeRefCounted<WmDesksPrivateRemoveDeskFunction>();
  auto error = api_test_utils::RunFunctionAndReturnError(
      remove_desk_function.get(), R"(["invalid-id"])", browser()->profile());

  EXPECT_EQ(error, "InvalidIdError");
  histogram_tester.ExpectBucketCount("Ash.DeskApi.RemoveDesk.Result", 0, 1);
}

// Tests switch desks failed.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, SwitchDeskWithInvalidIdTest) {
  base::HistogramTester histogram_tester;
  // Switches to the current desk.
  auto switch_desk_function =
      base::MakeRefCounted<WmDesksPrivateSwitchDeskFunction>();
  auto error = api_test_utils::RunFunctionAndReturnError(
      switch_desk_function.get(), R"(["invalid-id"])", browser()->profile());

  EXPECT_EQ(error, "InvalidIdError");
  histogram_tester.ExpectBucketCount("Ash.DeskApi.SwitchDesk.Result", 0, 1);
}

// Tests set all desks with invalid ID.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest,
                       SetAllDesksWindowWithInvalidIdTest) {
  base::HistogramTester histogram_tester;
  // Switches to the current desk.
  auto all_desk_function =
      base::MakeRefCounted<WmDesksPrivateSetWindowPropertiesFunction>();
  auto error = api_test_utils::RunFunctionAndReturnError(
      all_desk_function.get(), R"([123,{"allDesks":true}])",
      browser()->profile());

  EXPECT_EQ(error, "ResourceNotFoundError");
  histogram_tester.ExpectBucketCount("Ash.DeskApi.AllDesk.Result", 0, 1);
}

// Tests save and recall a desk.
// TODO(crbug.com/40902046): Test is flaky.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, DISABLED_SaveAndRecallDeskTest) {
  // Save a desk.
  auto save_desk_function =
      base::MakeRefCounted<WmDesksPrivateSaveActiveDeskFunction>();

  ash::DeskSwitchAnimationWaiter save_desk_waiter;
  // Asserts no error.
  auto result = api_test_utils::RunFunctionAndReturnSingleResult(
      save_desk_function.get(), R"([])", browser()->profile());
  EXPECT_TRUE(result->is_dict());
  auto desk_id = result->GetDict().Find("savedDeskUuid")->GetString();
  EXPECT_TRUE(base::Uuid::ParseCaseInsensitive(desk_id).is_valid());

  // Waiting for desk launch animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    save_desk_waiter.Wait();
  }

  // List saved desks.
  auto list_desk_function =
      base::MakeRefCounted<WmDesksPrivateGetSavedDesksFunction>();

  // Asserts no error.
  auto result_1 = api_test_utils::RunFunctionAndReturnSingleResult(
      list_desk_function.get(), R"([])", browser()->profile());
  EXPECT_TRUE(result_1->is_list());
  EXPECT_EQ(1u, result_1->GetList().size());
  EXPECT_TRUE(result_1->GetList().front().GetDict().Find("savedDeskUuid"));
  EXPECT_TRUE(result_1->GetList().front().GetDict().Find("savedDeskName"));
  EXPECT_TRUE(result_1->GetList().front().GetDict().Find("savedDeskType"));
  ash::DeskSwitchAnimationWaiter recall_desk_waiter;
  // Recall a desk.
  auto recall_desk_function =
      base::MakeRefCounted<WmDesksPrivateRecallSavedDeskFunction>();
  auto desk_id_1 = api_test_utils::RunFunctionAndReturnSingleResult(
      recall_desk_function.get(), R"([")" + desk_id + R"("])",
      browser()->profile());
  EXPECT_TRUE(desk_id_1->is_string());
  EXPECT_TRUE(
      base::Uuid::ParseCaseInsensitive(desk_id_1->GetString()).is_valid());

  // Waiting for desk removal animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    recall_desk_waiter.Wait();
  }
}

// Tests save and delete a desk.
// TODO(crbug.com/40902046): Flaky on linux-chromeos-rel.
#if defined(NDEBUG)
#define MAYBE_SaveAndDeleteDeskTest DISABLED_SaveAndDeleteDeskTest
#else
#define MAYBE_SaveAndDeleteDeskTest SaveAndDeleteDeskTest
#endif
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, MAYBE_SaveAndDeleteDeskTest) {
  // Save a desk.
  auto save_desk_function =
      base::MakeRefCounted<WmDesksPrivateSaveActiveDeskFunction>();

  ash::DeskSwitchAnimationWaiter save_desk_waiter;
  // Asserts no error.
  auto result = api_test_utils::RunFunctionAndReturnSingleResult(
      save_desk_function.get(), R"([])", browser()->profile());
  EXPECT_TRUE(result->is_dict());
  auto desk_id = result->GetDict().Find("savedDeskUuid")->GetString();
  EXPECT_TRUE(base::Uuid::ParseCaseInsensitive(desk_id).is_valid());

  // Waiting for desk launch animation to settle
  if (ash::DesksController::Get()->AreDesksBeingModified()) {
    save_desk_waiter.Wait();
  }

  // Delete a saved desk.
  auto deleted_saved_desk_function =
      base::MakeRefCounted<WmDesksPrivateDeleteSavedDeskFunction>();
  api_test_utils::RunFunctionAndReturnSingleResult(
      deleted_saved_desk_function.get(), R"([")" + desk_id + R"("])",
      browser()->profile());
}

// Tests retrieve desk with deskID.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, GetDeskByIDTest) {
  // Get current desk id.
  auto desk_id = api_test_utils::RunFunctionAndReturnSingleResult(
      base::MakeRefCounted<WmDesksPrivateGetActiveDeskFunction>().get(), "[]",
      browser()->profile());

  // Retrieve desk by Id.
  auto get_desk_by_id_function =
      base::MakeRefCounted<WmDesksPrivateGetDeskByIDFunction>();
  auto result = api_test_utils::RunFunctionAndReturnSingleResult(
      get_desk_by_id_function.get(), R"([")" + desk_id->GetString() + R"("])",
      browser()->profile());
  EXPECT_TRUE(result->is_dict());
  auto* desk_id_1 = result->GetDict().Find("deskUuid");
  auto* desk_name = result->GetDict().Find("deskName");
  ASSERT_TRUE(desk_id_1->is_string());
  EXPECT_EQ(desk_id->GetString(), desk_id_1->GetString());
  EXPECT_TRUE(desk_name->is_string());
}

// Tests retrieve desk with invalid deskID.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, GetDeskByInvalidIDTest) {
  // Retrieve desk by Id.
  auto get_desk_by_id_function =
      base::MakeRefCounted<WmDesksPrivateGetDeskByIDFunction>();
  auto error = api_test_utils::RunFunctionAndReturnError(
      get_desk_by_id_function.get(), R"(["invalid-id"])", browser()->profile());
  EXPECT_EQ(error, "InvalidIdError");
}

// Tests retrieve desk with non-exist deskID.
IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, GetDeskByNonExistIDTest) {
  auto desk_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  // Retrieve desk by Id.
  auto get_desk_by_id_function =
      base::MakeRefCounted<WmDesksPrivateGetDeskByIDFunction>();
  auto error = api_test_utils::RunFunctionAndReturnError(
      get_desk_by_id_function.get(), R"([")" + desk_id + R"("])",
      browser()->profile());
  EXPECT_EQ(error, "ResourceNotFoundError");
}

}  // namespace extensions
