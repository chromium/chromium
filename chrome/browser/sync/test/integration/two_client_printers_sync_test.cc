// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string_view>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ash/printing/printers_sync_bridge.h"
#include "chrome/browser/sync/test/integration/printers_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using printers_helper::AddPrinter;
using printers_helper::AllProfilesContainSamePrinters;
using printers_helper::CreateTestPrinter;
using printers_helper::CreateTestPrinterSpecifics;
using printers_helper::EditPrinterDescription;
using printers_helper::GetPrinterCount;
using printers_helper::GetPrinterStore;
using printers_helper::PrintersMatchChecker;
using printers_helper::RemovePrinter;
using ::testing::EndsWith;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StartsWith;

constexpr char kOverwrittenDescription[] = "I should not show up";
constexpr char kLatestDescription[] = "YAY!  More recent changes win!";

class TwoClientPrintersSyncTest : public SyncTest {
 public:
  TwoClientPrintersSyncTest() : SyncTest(TWO_CLIENT) {}

  TwoClientPrintersSyncTest(const TwoClientPrintersSyncTest&) = delete;
  TwoClientPrintersSyncTest& operator=(const TwoClientPrintersSyncTest&) =
      delete;

  ~TwoClientPrintersSyncTest() override = default;

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    CHECK(!UseVerifier());
    printers_helper::WaitForPrinterStoreToLoad(GetProfile(0));
    printers_helper::WaitForPrinterStoreToLoad(GetProfile(1));
    return true;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest, NoPrinters) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(PrintersMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest, OnePrinter) {
  ASSERT_TRUE(SetupSync());

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(2));

  ASSERT_TRUE(PrintersMatchChecker().Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest, SimultaneousAdd) {
  ASSERT_TRUE(SetupSync());

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(1));
  AddPrinter(GetPrinterStore(1), CreateTestPrinter(2));

  // Each store is guaranteed to have 1 printer because the tests run on the UI
  // thread.  ApplyIncrementalSyncChanges happens after we wait on the checker.
  ASSERT_EQ(1, GetPrinterCount(0));
  ASSERT_EQ(1, GetPrinterCount(1));

  ASSERT_TRUE(PrintersMatchChecker().Wait());
  EXPECT_EQ(2, GetPrinterCount(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest, RemovePrinter) {
  ASSERT_TRUE(SetupSync());

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(1));
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(2));
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(3));

  // Verify the profiles have the same printers
  ASSERT_TRUE(PrintersMatchChecker().Wait());
  EXPECT_EQ(3, GetPrinterCount(0));

  // Remove printer 2 from store 1
  RemovePrinter(GetPrinterStore(1), 2);

  ASSERT_TRUE(PrintersMatchChecker().Wait());
  EXPECT_EQ(2, GetPrinterCount(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest, RemoveAndEditPrinters) {
  const std::string updated_description = "Testing changes";

  ASSERT_TRUE(SetupSync());

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(1));
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(2));
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(3));

  // Verify the profiles have the same printers
  ASSERT_TRUE(PrintersMatchChecker().Wait());
  EXPECT_EQ(3, GetPrinterCount(0));

  // Edit printer 1 from store 0
  ASSERT_TRUE(
      EditPrinterDescription(GetPrinterStore(0), 1, updated_description));

  // Remove printer 2 from store 1
  RemovePrinter(GetPrinterStore(1), 2);

  ASSERT_TRUE(PrintersMatchChecker().Wait());
  EXPECT_EQ(2, GetPrinterCount(0));
  EXPECT_EQ(updated_description,
            GetPrinterStore(1)->GetSavedPrinters()[0].description());
}

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest, ConflictResolution) {
  ASSERT_TRUE(SetupSync());

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(0));
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(2));
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(3));

  // Store 0 and 1 have 3 printers now.
  ASSERT_TRUE(PrintersMatchChecker().Wait());

  // Client 1 makes a local change.
  ASSERT_TRUE(
      EditPrinterDescription(GetPrinterStore(1), 0, kOverwrittenDescription));

  // Wait for a non-zero period (200ms) for modification timestamps to differ.
  base::PlatformThread::Sleep(base::Milliseconds(200));

  // Client 0 is paused to make this test deterministic (client 1 commits
  // first).
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();

  // Client 0 makes a change while paused.
  ASSERT_TRUE(
      EditPrinterDescription(GetPrinterStore(0), 0, kLatestDescription));

  // We must wait until the sync cycle is completed before client 0 resumes in
  // order to make the outcome of conflict resolution deterministic (needed due
  // to lack of a strong consistency model on the server).
  ASSERT_TRUE(SyncServiceImplHarness::AwaitQuiescence({GetClient(1)}));

  ASSERT_EQ(GetPrinterStore(0)->GetSavedPrinters()[0].description(),
            kLatestDescription);
  ASSERT_EQ(GetPrinterStore(1)->GetSavedPrinters()[0].description(),
            kOverwrittenDescription);

  // Client 0 gets unpaused, which results in a conflict (local wins).
  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();

  // Run tasks until the most recent update has been applied to all stores.
  ASSERT_TRUE(PrintersMatchChecker().Wait());

  EXPECT_EQ(GetPrinterStore(0)->GetSavedPrinters()[0].description(),
            kLatestDescription);
  EXPECT_EQ(GetPrinterStore(1)->GetSavedPrinters()[0].description(),
            kLatestDescription);
}

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest,
                       ConflictResolutionWithStrongConsistency) {
  ASSERT_TRUE(SetupSync());
  GetFakeServer()->EnableStrongConsistencyWithConflictDetectionModel();

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(0));
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(2));
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(3));

  // Store 0 and 1 have 3 printers now.
  ASSERT_TRUE(PrintersMatchChecker().Wait());

  // Client 1 makes a local change.
  ASSERT_TRUE(
      EditPrinterDescription(GetPrinterStore(1), 0, kOverwrittenDescription));

  // Wait for a non-zero period (200ms) for modification timestamps to differ.
  base::PlatformThread::Sleep(base::Milliseconds(200));

  // Client 0 makes a change to the same printer.
  ASSERT_TRUE(
      EditPrinterDescription(GetPrinterStore(0), 0, kLatestDescription));

  // Run tasks until the most recent update has been applied to all stores.
  // One of the two clients (the second one committing) will be requested by the
  // server to resolve the conflict and recommit. The custom conflict resolution
  // as implemented in PrintersSyncBridge::ResolveConflict() should guarantee
  // that the one with latest modification timestamp (kLatestDescription) wins,
  // which can mean local wins or remote wins, depending on which client is
  // involved.
  ASSERT_TRUE(PrintersMatchChecker().Wait());

  EXPECT_EQ(GetPrinterStore(0)->GetSavedPrinters()[0].description(),
            kLatestDescription);
  EXPECT_EQ(GetPrinterStore(1)->GetSavedPrinters()[0].description(),
            kLatestDescription);
}

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest, SimpleMerge) {
  ASSERT_TRUE(SetupClients());
  base::RunLoop().RunUntilIdle();

  // Store 0 has the even printers
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(0));
  AddPrinter(GetPrinterStore(0), CreateTestPrinter(2));

  // Store 1 has the odd printers
  AddPrinter(GetPrinterStore(1), CreateTestPrinter(1));
  AddPrinter(GetPrinterStore(1), CreateTestPrinter(3));

  ASSERT_TRUE(SetupSync());

  // Stores should contain the same values now.
  EXPECT_EQ(4, GetPrinterCount(0));
  EXPECT_TRUE(AllProfilesContainSamePrinters());
}

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest, MakeAndModelMigration) {
  ASSERT_TRUE(SetupClients());
  const char kMake[] = "make";
  const char kModel[] = "model";

  // Initialize sync bridge with test printer.
  std::unique_ptr<sync_pb::PrinterSpecifics> printer =
      CreateTestPrinterSpecifics(0);
  const std::string spec_printer_id = printer->id();
  printer->set_manufacturer(kMake);
  printer->set_model(kModel);
  ash::PrintersSyncBridge* bridge = GetPrinterStore(0)->GetSyncBridge();
  bridge->AddPrinter(std::move(printer));

  // Confirm that the bridge is not migrated.
  std::optional<sync_pb::PrinterSpecifics> spec_printer =
      bridge->GetPrinter(spec_printer_id);
  ASSERT_TRUE(spec_printer);
  ASSERT_THAT(spec_printer->make_and_model(), IsEmpty());

  ASSERT_TRUE(SetupSync());
  spec_printer = bridge->GetPrinter(spec_printer_id);
  ASSERT_TRUE(spec_printer);

  std::string_view make_and_model = spec_printer->make_and_model();
  EXPECT_THAT(make_and_model, Not(IsEmpty()));
  EXPECT_THAT(make_and_model, StartsWith(kMake));
  EXPECT_THAT(make_and_model, EndsWith(kModel));
}

IN_PROC_BROWSER_TEST_F(TwoClientPrintersSyncTest,
                       InvalidPpdReferenceResolution) {
  ASSERT_TRUE(SetupClients());

  // Initialize sync bridge with test printer.
  std::unique_ptr<sync_pb::PrinterSpecifics> printer =
      CreateTestPrinterSpecifics(0);
  const std::string spec_printer_id = printer->id();

  auto ppd_ref = std::make_unique<sync_pb::PrinterPPDReference>();
  ppd_ref->set_autoconf(true);
  ppd_ref->set_user_supplied_ppd_url("file://fake_ppd_url");
  printer->set_allocated_ppd_reference(ppd_ref.release());

  ash::PrintersSyncBridge* bridge = GetPrinterStore(0)->GetSyncBridge();
  bridge->AddPrinter(std::move(printer));

  // Confirm that the bridge is not migrated.
  std::optional<sync_pb::PrinterSpecifics> spec_printer =
      bridge->GetPrinter(spec_printer_id);
  ASSERT_TRUE(spec_printer);
  ASSERT_TRUE(spec_printer->has_ppd_reference());
  sync_pb::PrinterPPDReference spec_ppd_ref = spec_printer->ppd_reference();
  ASSERT_TRUE(spec_ppd_ref.autoconf());
  ASSERT_TRUE(spec_ppd_ref.has_user_supplied_ppd_url());

  // Perform sync.
  ASSERT_TRUE(SetupSync());
  spec_printer = bridge->GetPrinter(spec_printer_id);
  ASSERT_TRUE(spec_printer);
  ASSERT_TRUE(spec_printer->has_ppd_reference());

  spec_ppd_ref = spec_printer->ppd_reference();
  EXPECT_FALSE(spec_ppd_ref.autoconf());
  EXPECT_TRUE(spec_ppd_ref.has_user_supplied_ppd_url());
}
