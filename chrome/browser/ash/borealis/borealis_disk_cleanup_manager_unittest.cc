// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_disk_cleanup_manager.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

constexpr char kTestCryptohomeId[] = "test-cryptohome-id";
constexpr char kHasAllocatedDiskHistogram[] = "Borealis.HasAllocatedDisk";

}  // namespace

class BorealisDiskCleanupManagerTest
    : public testing::Test,
      protected guest_os::FakeVmServicesHelper {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BorealisDiskCleanupManagerTest, NoBorealisVmDoesNotRecordHistogram) {
  vm_tools::concierge::ListVmDisksResponse response;
  response.set_success(true);
  FakeConciergeClient()->set_list_vm_disks_response(response);

  base::HistogramTester histogram_tester;
  BorealisDiskCleanupManager manager(kTestCryptohomeId);
  base::RunLoop run_loop;
  manager.CheckDiskSpace(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(FakeConciergeClient()->list_vm_disks_call_count(), 1);
  histogram_tester.ExpectTotalCount(kHasAllocatedDiskHistogram, 0);
}

TEST_F(BorealisDiskCleanupManagerTest, OtherVmImageDoesNotRecordHistogram) {
  vm_tools::concierge::ListVmDisksResponse response;
  response.set_success(true);
  auto* image = response.add_images();
  image->set_name("termina");
  image->set_size(10ULL * 1024 * 1024 * 1024);
  FakeConciergeClient()->set_list_vm_disks_response(response);

  base::HistogramTester histogram_tester;
  BorealisDiskCleanupManager manager(kTestCryptohomeId);
  base::RunLoop run_loop;
  manager.CheckDiskSpace(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(FakeConciergeClient()->list_vm_disks_call_count(), 1);
  histogram_tester.ExpectTotalCount(kHasAllocatedDiskHistogram, 0);
}

TEST_F(BorealisDiskCleanupManagerTest, ZeroDiskAllocatedRecordsFalse) {
  vm_tools::concierge::ListVmDisksResponse response;
  response.set_success(true);
  auto* image = response.add_images();
  image->set_name("borealis");
  image->set_size(0);
  FakeConciergeClient()->set_list_vm_disks_response(response);

  base::HistogramTester histogram_tester;
  BorealisDiskCleanupManager manager(kTestCryptohomeId);
  base::RunLoop run_loop;
  manager.CheckDiskSpace(run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(kHasAllocatedDiskHistogram, false, 1);
}

TEST_F(BorealisDiskCleanupManagerTest, AllocatedDiskRecordsTrue) {
  vm_tools::concierge::ListVmDisksResponse response;
  response.set_success(true);
  auto* image = response.add_images();
  image->set_name("borealis");
  image->set_size(10ULL * 1024 * 1024 * 1024);  // 10 GiB
  FakeConciergeClient()->set_list_vm_disks_response(response);

  base::HistogramTester histogram_tester;
  BorealisDiskCleanupManager manager(kTestCryptohomeId);
  base::RunLoop run_loop;
  manager.CheckDiskSpace(run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(kHasAllocatedDiskHistogram, true, 1);
}

TEST_F(BorealisDiskCleanupManagerTest, ListVmDisksFailureLogsAndDoesNotRecord) {
  vm_tools::concierge::ListVmDisksResponse response;
  response.set_success(false);
  response.set_failure_reason("Failed to access storage");
  FakeConciergeClient()->set_list_vm_disks_response(response);

  base::HistogramTester histogram_tester;
  BorealisDiskCleanupManager manager(kTestCryptohomeId);
  base::RunLoop run_loop;
  manager.CheckDiskSpace(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(FakeConciergeClient()->list_vm_disks_call_count(), 1);
  histogram_tester.ExpectTotalCount(kHasAllocatedDiskHistogram, 0);
}

}  // namespace borealis
