// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_disk.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {
namespace disk {

class CrostiniDiskTest : public testing::Test {
 public:
  CrostiniDiskTest() = default;
  ~CrostiniDiskTest() override = default;

 protected:
  // A wrapper for OnListVmDisks which returns the result.
  std::unique_ptr<CrostiniDiskInfo> OnListVmDisksWithResult(
      const char* vm_name,
      int64_t free_space,
      std::optional<vm_tools::concierge::ListVmDisksResponse>
          list_disks_response) {
    std::unique_ptr<CrostiniDiskInfo> result;
    auto store = base::BindLambdaForTesting(
        [&result](std::unique_ptr<CrostiniDiskInfo> info) {
          result = std::move(info);
        });

    OnListVmDisks(store, vm_name, free_space, list_disks_response);
    // OnListVmDisks is synchronous and runs the callback upon finishing, so by
    // the time it returns we know that result has been stored.
    return result;
  }
};

class CrostiniDiskTestDbus : public CrostiniDiskTest {
 public:
  CrostiniDiskTestDbus() {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    fake_concierge_client_ = ash::FakeConciergeClient::Get();
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile_.get());
    CrostiniManager::GetForProfile(profile_.get())
        ->set_skip_restart_for_testing();
  }

  void TearDown() override {
    test_helper_.reset();
    profile_.reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

 protected:
  // A wrapper for ResizeCrostiniDisk which returns the result.
  bool OnResizeWithResult(Profile* profile,
                          const char* vm_name,
                          int64_t size_bytes) {
    base::test::TestFuture<bool> result_future;
    ResizeCrostiniDisk(profile, vm_name, size_bytes,
                       result_future.GetCallback());
    return result_future.Get();
  }

  Profile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<ash::FakeConciergeClient, DanglingUntriaged> fake_concierge_client_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;
};

TEST_F(CrostiniDiskTest, NonResizeableDiskReturnsEarly) {
  vm_tools::concierge::ListVmDisksResponse response;
  response.set_success(true);
  auto* image = response.add_images();
  image->set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_QCOW2);
  image->set_name("vm_name");

  auto disk_info = OnListVmDisksWithResult("vm_name", 0, response);
  ASSERT_TRUE(disk_info);
  EXPECT_FALSE(disk_info->can_resize);
}

TEST_F(CrostiniDiskTest, CallbackGetsEmptyInfoOnError) {
  auto disk_info_nullopt = OnListVmDisksWithResult("vm_name", 0, std::nullopt);
  EXPECT_FALSE(disk_info_nullopt);

  vm_tools::concierge::ListVmDisksResponse failure_response;
  failure_response.set_success(false);
  auto disk_info_failure =
      OnListVmDisksWithResult("vm_name", 0, failure_response);
  EXPECT_FALSE(disk_info_failure);

  vm_tools::concierge::ListVmDisksResponse no_matching_disks_response;
  auto* image = no_matching_disks_response.add_images();
  no_matching_disks_response.set_success(true);
  image->set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_QCOW2);
  image->set_name("wrong_name");

  auto disk_info_no_disks =
      OnListVmDisksWithResult("vm_name", 0, no_matching_disks_response);
  EXPECT_FALSE(disk_info_no_disks);
}

TEST_F(CrostiniDiskTest, IsUserChosenSizeIsReportedCorrectly) {
  vm_tools::concierge::ListVmDisksResponse response;
  auto* image = response.add_images();
  response.set_success(true);
  image->set_name("vm_name");
  image->set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW);
  image->set_user_chosen_size(true);
  image->set_min_size(1);

  const int64_t available_bytes = kDiskHeadroomBytes + kMinimumDiskSizeBytes;
  auto disk_info_user_size =
      OnListVmDisksWithResult("vm_name", available_bytes, response);

  ASSERT_TRUE(disk_info_user_size);
  EXPECT_TRUE(disk_info_user_size->can_resize);
  EXPECT_TRUE(disk_info_user_size->is_user_chosen_size);

  image->set_user_chosen_size(false);

  auto disk_info_not_user_size =
      OnListVmDisksWithResult("vm_name", available_bytes, response);

  ASSERT_TRUE(disk_info_not_user_size);
  EXPECT_TRUE(disk_info_not_user_size->can_resize);
  EXPECT_FALSE(disk_info_not_user_size->is_user_chosen_size);
}

TEST_F(CrostiniDiskTest, AreTicksCalculated) {
  // The actual tick calculation has its own unit tests, we just check that we
  // get something that looks sane for given values.
  vm_tools::concierge::ListVmDisksResponse response;
  auto* image = response.add_images();
  response.set_success(true);
  image->set_name("vm_name");
  image->set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW);
  image->set_min_size(1000);
  image->set_size(kMinimumDiskSizeBytes);

  auto disk_info =
      OnListVmDisksWithResult("vm_name", 100 + kDiskHeadroomBytes, response);

  ASSERT_TRUE(disk_info);
  EXPECT_EQ(disk_info->ticks.front()->value, kMinimumDiskSizeBytes);
}

TEST_F(CrostiniDiskTest, DefaultIsCurrentValue) {
  vm_tools::concierge::ListVmDisksResponse response;
  auto* image = response.add_images();
  response.set_success(true);
  image->set_name("vm_name");
  image->set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW);
  image->set_min_size(1000);
  image->set_size(3 * kGiB);
  auto disk_info =
      OnListVmDisksWithResult("vm_name", 111 * kDiskHeadroomBytes, response);
  ASSERT_TRUE(disk_info);

  ASSERT_TRUE(disk_info->ticks.size() > 3);
  EXPECT_EQ(disk_info->ticks.at(disk_info->default_index)->value, 3 * kGiB);
  EXPECT_LT(disk_info->ticks.at(disk_info->default_index - 1)->value, 3 * kGiB);
  EXPECT_GT(disk_info->ticks.at(disk_info->default_index + 1)->value, 3 * kGiB);
}

// Numbers taken from crbug/1126705.
TEST_F(CrostiniDiskTest, AllocatedAboveMax) {
  vm_tools::concierge::ListVmDisksResponse response;
  auto* image = response.add_images();
  response.set_success(true);
  image->set_name("vm_name");
  image->set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW);
  image->set_min_size(3260022784);
  image->set_size(459561652224);
  auto disk_info = OnListVmDisksWithResult("vm_name", 1120739328, response);
  ASSERT_TRUE(disk_info);

  ASSERT_TRUE(disk_info->ticks.size() > 3);
  ASSERT_GE(disk_info->default_index, 0);
  ASSERT_EQ(static_cast<size_t>(disk_info->default_index),
            disk_info->ticks.size() - 1);
  EXPECT_EQ(static_cast<uint64_t>(
                disk_info->ticks.at(disk_info->default_index)->value),
            image->size());
}

TEST_F(CrostiniDiskTest, AmountOfFreeDiskSpaceFailureIsHandled) {
  std::unique_ptr<CrostiniDiskInfo> disk_info;
  auto store_info =
      base::BindLambdaForTesting([&](std::unique_ptr<CrostiniDiskInfo> info) {
        disk_info = std::move(info);
      });

  OnAmountOfFreeDiskSpace(store_info, nullptr, "vm_name", 0);
  EXPECT_FALSE(disk_info);
}

TEST_F(CrostiniDiskTest, VMRunningFailureIsHandled) {
  std::unique_ptr<CrostiniDiskInfo> disk_info;
  auto store_info =
      base::BindLambdaForTesting([&](std::unique_ptr<CrostiniDiskInfo> info) {
        disk_info = std::move(info);
      });

  OnCrostiniSufficientlyRunning(store_info, nullptr, "vm_name", 0,
                                CrostiniResult::VM_START_FAILED);
  EXPECT_FALSE(disk_info);
}

TEST_F(CrostiniDiskTestDbus, DiskResizeImmediateFailureReportsFailure) {
  vm_tools::concierge::ResizeDiskImageResponse response;
  response.set_status(vm_tools::concierge::DiskImageStatus::DISK_STATUS_FAILED);
  fake_concierge_client_->set_resize_disk_image_response(response);

  auto result = OnResizeWithResult(profile(), "vm_name", 12345);

  EXPECT_EQ(result, false);
}

TEST_F(CrostiniDiskTestDbus, DiskResizeEventualFailureReportsFailure) {
  vm_tools::concierge::ResizeDiskImageResponse response;
  vm_tools::concierge::DiskImageStatusResponse in_progress;
  vm_tools::concierge::DiskImageStatusResponse failed;
  response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS);
  in_progress.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS);
  failed.set_status(vm_tools::concierge::DiskImageStatus::DISK_STATUS_FAILED);
  fake_concierge_client_->set_resize_disk_image_response(response);
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals{in_progress,
                                                                    failed};
  fake_concierge_client_->set_disk_image_status_signals(signals);

  auto result = OnResizeWithResult(profile(), "vm_name", 12345);

  EXPECT_EQ(result, false);
}

TEST_F(CrostiniDiskTestDbus, DiskResizeEventualSuccessReportsSuccess) {
  vm_tools::concierge::ResizeDiskImageResponse response;
  vm_tools::concierge::DiskImageStatusResponse in_progress;
  vm_tools::concierge::DiskImageStatusResponse resized;
  response.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS);
  in_progress.set_status(
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_IN_PROGRESS);
  resized.set_status(vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED);
  fake_concierge_client_->set_resize_disk_image_response(response);
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals{in_progress,
                                                                    resized};
  fake_concierge_client_->set_disk_image_status_signals(signals);

  auto result = OnResizeWithResult(profile(), "vm_name", 12345);

  EXPECT_EQ(result, true);
}

TEST_F(CrostiniDiskTestDbus, DiskResizeNegativeHeadroom) {
  vm_tools::concierge::ListVmDisksResponse response;
  auto* image = response.add_images();
  response.set_success(true);
  image->set_name("vm_name");
  image->set_image_type(vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW);
  image->set_min_size(1000);
  image->set_size(3 * kGiB);
  auto disk_info =
      OnListVmDisksWithResult("vm_name", kDiskHeadroomBytes - 1, response);
  ASSERT_TRUE(disk_info);

  ASSERT_TRUE(disk_info->ticks.size() > 0);
  ASSERT_EQ(disk_info->ticks.at(disk_info->ticks.size() - 1)->value, 3 * kGiB);
}

TEST_F(CrostiniDiskTest, GetTicksForDiskSizeInvalidInputsNoTicks) {
  const std::vector<int64_t> empty = {};
  // If min_size > available_space there's no solution, so we expect an empty
  // range.
  EXPECT_THAT(GetTicksForDiskSize(100, 10), testing::ContainerEq(empty));

  // If any of the inputs are negative we return an empty range.
  EXPECT_THAT(GetTicksForDiskSize(100, -100), testing::ContainerEq(empty));
  EXPECT_THAT(GetTicksForDiskSize(-100, 100), testing::ContainerEq(empty));
  EXPECT_THAT(GetTicksForDiskSize(-100, -10), testing::ContainerEq(empty));
}

TEST_F(CrostiniDiskTest, GetTicksForDiskSizeRoundEnds) {
  // With 1000 GiB - epsilon of free space we should round to 1 GiB increments.
  // Our top should be rounded down, and bottom rounded up (since they aren't on
  // 1GiB).
  auto ticks = GetTicksForDiskSize(1, 1000 * kGiB - 1);
  EXPECT_EQ(ticks.front(), 1 * kGiB);
  EXPECT_EQ(ticks.back(), 999 * kGiB);
}

TEST_F(CrostiniDiskTest, GetTicksForDiskSizeExactEnds) {
  // With 1000 GiB of free space we should round to 1 GiB increments. Since our
  // max and min are on 1GIB increments already, they should not be rounded.
  auto ticks = GetTicksForDiskSize(0, 1000 * kGiB);
  EXPECT_EQ(ticks.front(), 0 * kGiB);
  EXPECT_EQ(ticks.back(), 1000 * kGiB);
}

TEST_F(CrostiniDiskTest, GetTicksForDiskSizeIncrements) {
  // We target 400'ish ticks on the slider (implementation detail). With that
  // granularity we should have increments of:
  //  1.0 GiB for >= 400kGiB
  //  0.5 GiB for >= 200kGiB && < 400kGiB
  //  0.2 GiB for >= 80GiB && < 200GiB
  //  0.1 GiB for < 80 GiB
  auto ticks10 = GetTicksForDiskSize(0, 401 * kGiB);
  auto ticks05 = GetTicksForDiskSize(0, 399 * kGiB);
  auto ticks02 = GetTicksForDiskSize(0, 81 * kGiB);
  auto ticks01 = GetTicksForDiskSize(0, 79 * kGiB);

  EXPECT_FLOAT_EQ(ticks10[0] + 1.0 * kGiB, double(ticks10[1]));
  EXPECT_FLOAT_EQ(ticks05[0] + 0.5 * kGiB, double(ticks05[1]));
  EXPECT_FLOAT_EQ(ticks02[0] + 0.2 * kGiB, double(ticks02[1]));
  EXPECT_FLOAT_EQ(ticks01[0] + 0.1 * kGiB, double(ticks01[1]));
}

TEST_F(CrostiniDiskTest, GetTicksForDiskSizeMinimalSpace) {
  // Currently our minimum increment is 0.1 GiB. This means that if they have
  // <(min + 0.1) GiB available their only option is min.
  auto expected = std::vector<int64_t>{2 * kGiB};
  EXPECT_THAT(GetTicksForDiskSize(2 * kGiB, 2 * kGiB + 0.09 * kGiB),
              testing::ContainerEq(expected));
}

TEST_F(CrostiniDiskTest, GetTicksForDiskSizeSmallRangeNonZeroStart) {
  // 20 ticks for 1 GiB, smallest interval is 0.1GiB so we should end up with
  // only 11 0.1 GiB ticks. For bonus coverage, start at non-zero/.
  auto ticks = GetTicksForDiskSize(2 * kGiB, 3 * kGiB, 20);
  std::vector<int64_t> expected = {
      int64_t(2.0 * kGiB), int64_t(2.1 * kGiB), int64_t(2.2 * kGiB),
      int64_t(2.3 * kGiB), int64_t(2.4 * kGiB), int64_t(2.5 * kGiB),
      int64_t(2.6 * kGiB), int64_t(2.7 * kGiB), int64_t(2.8 * kGiB),
      int64_t(2.9 * kGiB), int64_t(3.0 * kGiB)};
  ASSERT_EQ(ticks.size(), expected.size());
  for (size_t n = 0; n < expected.size(); n++) {
    EXPECT_FLOAT_EQ(ticks[n], expected[n]);
  }
}

TEST_F(CrostiniDiskTest, GetTicksForDiskSizeLargeRange) {
  // 5 ticks for 7 GiB, largest interval is 1GiB so we should end up with 8
  // 0.1 GiB ticks. For bonus coverage, start at non-zero non-round.
  auto ticks = GetTicksForDiskSize(13 * kGiB - 5678, 20 * kGiB + 1234, 5);
  std::vector<int64_t> expected = {13 * kGiB, 14 * kGiB, 15 * kGiB, 16 * kGiB,
                                   17 * kGiB, 18 * kGiB, 19 * kGiB, 20 * kGiB};
  ASSERT_EQ(ticks.size(), expected.size());
  for (size_t n = 0; n < expected.size(); n++) {
    EXPECT_FLOAT_EQ(ticks[n], expected[n]);
  }
}

}  // namespace disk
}  // namespace crostini
