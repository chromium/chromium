// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/ash/printing/printers_sync_bridge.h"
#include "chrome/browser/sync/test/integration/printers_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using printers_helper::AddPrinter;
using printers_helper::CreateTestPrinter;
using printers_helper::CreateTestPrinterSpecifics;
using printers_helper::EditPrinterDescription;
using printers_helper::GetPrinterCount;
using printers_helper::GetPrinterStore;
using printers_helper::MatchesPrinter;
using printers_helper::RemovePrinter;
using printers_helper::ServerPrinterMatchChecker;
using testing::ElementsAre;
using testing::IsEmpty;

namespace {

class SingleClientPrintersSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientPrintersSyncTest() : SyncTest(SINGLE_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      features_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }
  ~SingleClientPrintersSyncTest() override = default;

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    printers_helper::WaitForPrinterStoreToLoad(GetProfile(0));
    return true;
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SingleClientPrintersSyncTest,
    // TODO(crbug.com/509847617): Use GetSyncTestModes() when the sync
    // integration tests get parameterized on ChromeOS.
    testing::Values(SyncTest::SetupSyncMode::kSyncTransportOnly,
                    SyncTest::SetupSyncMode::kSyncTheFeature),
    testing::PrintToStringParamName());

// Verify that printers aren't added with a sync call.
IN_PROC_BROWSER_TEST_P(SingleClientPrintersSyncTest, NoPrinters) {
  ASSERT_TRUE(SetupSync());
  EXPECT_EQ(0, GetPrinterCount(0));
  EXPECT_TRUE(ServerPrinterMatchChecker(IsEmpty()).Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientPrintersSyncTest, SingleNewPrinter) {
  ASSERT_TRUE(SetupSync());

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(0));
  EXPECT_EQ(1, GetPrinterCount(0));

  EXPECT_TRUE(ServerPrinterMatchChecker(
                  ElementsAre(MatchesPrinter(CreateTestPrinter(0))))
                  .Wait());
}

// Verify editing a printer updates the server entity.
IN_PROC_BROWSER_TEST_P(SingleClientPrintersSyncTest, EditPrinter) {
  ASSERT_TRUE(SetupSync());

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(0));
  ASSERT_TRUE(ServerPrinterMatchChecker(
                  ElementsAre(MatchesPrinter(CreateTestPrinter(0))))
                  .Wait());

  ASSERT_TRUE(
      EditPrinterDescription(GetPrinterStore(0), 0, "Updated description"));

  EXPECT_EQ(1, GetPrinterCount(0));

  chromeos::Printer expected_printer = CreateTestPrinter(0);
  expected_printer.set_description("Updated description");
  EXPECT_TRUE(
      ServerPrinterMatchChecker(ElementsAre(MatchesPrinter(expected_printer)))
          .Wait());
}

// Verify that removing a printer works.
IN_PROC_BROWSER_TEST_P(SingleClientPrintersSyncTest, RemovePrinter) {
  ASSERT_TRUE(SetupSync());

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(0));
  ASSERT_EQ(1, GetPrinterCount(0));
  ASSERT_TRUE(ServerPrinterMatchChecker(
                  ElementsAre(MatchesPrinter(CreateTestPrinter(0))))
                  .Wait());

  RemovePrinter(GetPrinterStore(0), 0);
  EXPECT_EQ(0, GetPrinterCount(0));
  EXPECT_TRUE(ServerPrinterMatchChecker(IsEmpty()).Wait());
}

// Verify that merging data added before sync works.
IN_PROC_BROWSER_TEST_P(SingleClientPrintersSyncTest, AddBeforeSetup) {
  ASSERT_TRUE(SetupClients());

  AddPrinter(GetPrinterStore(0), CreateTestPrinter(0));
  ASSERT_EQ(1, GetPrinterCount(0));

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(ServerPrinterMatchChecker(
                  ElementsAre(MatchesPrinter(CreateTestPrinter(0))))
                  .Wait());
}

// Verify that adding a print server printer retains the print server URI.
IN_PROC_BROWSER_TEST_P(SingleClientPrintersSyncTest, AddPrintServerPrinter) {
  ASSERT_TRUE(SetupClients());
  const char kServerAddress[] = "ipp://192.168.1.1:631";

  // Initialize sync bridge with test printer.
  std::unique_ptr<sync_pb::PrinterSpecifics> printer =
      CreateTestPrinterSpecifics(0);
  const std::string spec_printer_id = printer->id();
  printer->set_print_server_uri(kServerAddress);
  ash::PrintersSyncBridge* bridge = GetPrinterStore(0)->GetSyncBridge();
  bridge->AddPrinter(std::move(printer));

  // Start the sync.
  ASSERT_TRUE(SetupSync());
  std::optional<sync_pb::PrinterSpecifics> spec_printer =
      bridge->GetPrinter(spec_printer_id);
  ASSERT_TRUE(spec_printer);

  // Verify that the print server address was saved correctly.
  EXPECT_EQ(kServerAddress, spec_printer->print_server_uri());
}

}  // namespace
