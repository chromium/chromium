// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_cleanup_manager.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace borealis {

namespace {

constexpr char kTestCryptohomeId[] = "test-cryptohome-id";
constexpr char kHasAllocatedDiskHistogram[] = "Borealis.HasAllocatedDisk";

class BorealisDiskCleanupManagerTest
    : public ChromeAshTestBase,
      protected guest_os::FakeVmServicesHelper {
 protected:
  std::unique_ptr<BorealisDiskCleanupManager> CreateManager() {
    return std::make_unique<BorealisDiskCleanupManager>(
        kTestCryptohomeId, profile_.GetPrefs(),
        guest_os::GuestOsRegistryServiceFactory::GetForProfile(&profile_));
  }

  // Runs CheckDiskSpace() on `manager` and waits for the result.
  void CheckDiskSpaceAndWait(BorealisDiskCleanupManager& manager) {
    base::RunLoop run_loop;
    manager.CheckDiskSpace(run_loop.QuitClosure());
    run_loop.Run();
  }

  void SetListVmDisksResponse(bool success,
                              std::string_view vm_name = {},
                              uint64_t size = 0) {
    vm_tools::concierge::ListVmDisksResponse response;
    response.set_success(success);
    if (!vm_name.empty()) {
      auto* image = response.add_images();
      image->set_name(std::string(vm_name));
      image->set_size(size);
    }
    FakeConciergeClient()->set_list_vm_disks_response(response);
  }

  TestingProfile profile_;
};

TEST_F(BorealisDiskCleanupManagerTest, NoBorealisVmDoesNotRecordHistogram) {
  SetListVmDisksResponse(/*success=*/true);

  base::HistogramTester histogram_tester;
  auto manager = CreateManager();
  CheckDiskSpaceAndWait(*manager);

  EXPECT_EQ(FakeConciergeClient()->list_vm_disks_call_count(), 1);
  histogram_tester.ExpectTotalCount(kHasAllocatedDiskHistogram, 0);
  EXPECT_FALSE(manager->dialog_widget_for_testing());
}

TEST_F(BorealisDiskCleanupManagerTest, OtherVmImageDoesNotRecordHistogram) {
  SetListVmDisksResponse(/*success=*/true, "termina",
                         10ULL * 1024 * 1024 * 1024);

  base::HistogramTester histogram_tester;
  auto manager = CreateManager();
  CheckDiskSpaceAndWait(*manager);

  EXPECT_EQ(FakeConciergeClient()->list_vm_disks_call_count(), 1);
  histogram_tester.ExpectTotalCount(kHasAllocatedDiskHistogram, 0);
  EXPECT_FALSE(manager->dialog_widget_for_testing());
}

TEST_F(BorealisDiskCleanupManagerTest,
       ZeroDiskAllocatedRecordsFalseAndNoDialog) {
  SetListVmDisksResponse(/*success=*/true, "borealis", /*size=*/0);

  base::HistogramTester histogram_tester;
  auto manager = CreateManager();
  CheckDiskSpaceAndWait(*manager);

  histogram_tester.ExpectUniqueSample(kHasAllocatedDiskHistogram, false, 1);
  EXPECT_FALSE(manager->dialog_widget_for_testing());
}

TEST_F(BorealisDiskCleanupManagerTest,
       AllocatedDiskRecordsTrueShowsDialogAndFreesDisk) {
  SetListVmDisksResponse(/*success=*/true, "borealis",
                         10ULL * 1024 * 1024 * 1024);  // 10 GiB
  profile_.GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, true);

  base::HistogramTester histogram_tester;
  auto manager = CreateManager();
  CheckDiskSpaceAndWait(*manager);

  histogram_tester.ExpectUniqueSample(kHasAllocatedDiskHistogram, true, 1);
  views::Widget* dialog = manager->dialog_widget_for_testing();
  ASSERT_TRUE(dialog);
  EXPECT_TRUE(dialog->IsVisible());

  // Accepting the dialog destroys the disk image, uninstalls the DLC,
  // clears the installed pref, and shows a completion notification.
  dialog->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !profile_.GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice);
  }));
  EXPECT_EQ(FakeConciergeClient()->destroy_disk_image_call_count(), 1);

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          BorealisDiskCleanupManager::kCleanupNotificationId);
  ASSERT_TRUE(notification);
  EXPECT_EQ(notification->title(), u"Steam on Chromebook");
  EXPECT_EQ(notification->message(), u"Freed 10.0 GB of disk space.");
}

TEST_F(BorealisDiskCleanupManagerTest, DialogClosedOnDestruction) {
  SetListVmDisksResponse(/*success=*/true, "borealis",
                         10ULL * 1024 * 1024 * 1024);

  auto manager = CreateManager();
  CheckDiskSpaceAndWait(*manager);
  views::Widget* dialog = manager->dialog_widget_for_testing();
  ASSERT_TRUE(dialog);
  base::WeakPtr<views::Widget> weak_dialog = dialog->GetWeakPtr();

  manager.reset();
  EXPECT_FALSE(weak_dialog);
}

TEST_F(BorealisDiskCleanupManagerTest,
       AllocatedDiskRecordsTrueDismissesDialogOnCancel) {
  SetListVmDisksResponse(/*success=*/true, "borealis",
                         10ULL * 1024 * 1024 * 1024);
  profile_.GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, true);

  auto manager = CreateManager();
  CheckDiskSpaceAndWait(*manager);

  views::Widget* dialog = manager->dialog_widget_for_testing();
  ASSERT_TRUE(dialog);
  EXPECT_TRUE(dialog->IsVisible());

  dialog->widget_delegate()->AsDialogDelegate()->CancelDialog();

  EXPECT_EQ(FakeConciergeClient()->destroy_disk_image_call_count(), 0);
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kBorealisInstalledOnDevice));
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          BorealisDiskCleanupManager::kCleanupNotificationId));
}

TEST_F(BorealisDiskCleanupManagerTest, DoesNotShowDuplicateDialog) {
  SetListVmDisksResponse(/*success=*/true, "borealis",
                         10ULL * 1024 * 1024 * 1024);

  auto manager = CreateManager();
  CheckDiskSpaceAndWait(*manager);
  views::Widget* dialog = manager->dialog_widget_for_testing();
  ASSERT_TRUE(dialog);

  // Calling CheckDiskSpace again while the dialog is open does not create a
  // new dialog.
  CheckDiskSpaceAndWait(*manager);
  EXPECT_EQ(manager->dialog_widget_for_testing(), dialog);
}

TEST_F(BorealisDiskCleanupManagerTest, ListVmDisksFailureLogsAndDoesNotRecord) {
  vm_tools::concierge::ListVmDisksResponse response;
  response.set_success(false);
  response.set_failure_reason("Failed to access storage");
  FakeConciergeClient()->set_list_vm_disks_response(response);

  base::HistogramTester histogram_tester;
  auto manager = CreateManager();
  CheckDiskSpaceAndWait(*manager);

  EXPECT_EQ(FakeConciergeClient()->list_vm_disks_call_count(), 1);
  histogram_tester.ExpectTotalCount(kHasAllocatedDiskHistogram, 0);
  EXPECT_FALSE(manager->dialog_widget_for_testing());
}

}  // namespace
}  // namespace borealis
