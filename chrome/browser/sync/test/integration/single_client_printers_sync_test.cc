// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/ash/printing/printers_sync_bridge.h"
#include "chrome/browser/sync/test/integration/printers_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/test/browser_test.h"

using printers_helper::AddPrinter;
using printers_helper::CreateTestPrinterSpecifics;
using printers_helper::EditPrinterDescription;
using printers_helper::GetPrinterCount;
using printers_helper::GetPrinterStore;
using printers_helper::GetVerifierPrinterCount;
using printers_helper::GetVerifierPrinterStore;
using printers_helper::ProfileContainsSamePrintersAsVerifier;
using printers_helper::RemovePrinter;

namespace {

class SingleClientPrintersSyncTest : public SyncTest {
 public:
  SingleClientPrintersSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPrintersSyncTest() override = default;

  bool SetupClients() override {
    if (!SyncTest::SetupClients()) {
      return false;
    }

    CHECK(UseVerifier());
    printers_helper::WaitForPrinterStoreToLoad(verifier());
    printers_helper::WaitForPrinterStoreToLoad(GetProfile(0));
    return true;
  }

  bool UseVerifier() override {
    // TODO(crbug.com/40724972): rewrite tests to not use verifier.
    return true;
  }
};

// Verify that printers aren't added with a sync call.
IN_PROC_BROWSER_TEST_F(SingleClientPrintersSyncTest, NoPrinters) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(ProfileContainsSamePrintersAsVerifier(0));
}

// Verify syncing doesn't randomly remove a printer.
IN_PROC_BROWSER_TEST_F(SingleClientPrintersSyncTest, SingleNewPrinter) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_EQ(0, GetVerifierPrinterCount());

  AddPrinter(GetPrinterStore(0), printers_helper::CreateTestPrinter(0));
  AddPrinter(GetVerifierPrinterStore(), printers_helper::CreateTestPrinter(0));
  ASSERT_EQ(1, GetPrinterCount(0));
  ASSERT_EQ(1, GetVerifierPrinterCount());

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_EQ(1, GetVerifierPrinterCount());
  EXPECT_TRUE(ProfileContainsSamePrintersAsVerifier(0));
}

// Verify editing a printer doesn't add it.
IN_PROC_BROWSER_TEST_F(SingleClientPrintersSyncTest, EditPrinter) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddPrinter(GetPrinterStore(0), printers_helper::CreateTestPrinter(0));
  AddPrinter(GetVerifierPrinterStore(), printers_helper::CreateTestPrinter(0));

  ASSERT_TRUE(
      EditPrinterDescription(GetPrinterStore(0), 0, "Updated description"));

  EXPECT_EQ(1, GetPrinterCount(0));
  EXPECT_EQ(1, GetVerifierPrinterCount());
  EXPECT_FALSE(ProfileContainsSamePrintersAsVerifier(0));
}

// Verify that removing a printer works.
IN_PROC_BROWSER_TEST_F(SingleClientPrintersSyncTest, RemovePrinter) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddPrinter(GetPrinterStore(0), printers_helper::CreateTestPrinter(0));
  EXPECT_EQ(1, GetPrinterCount(0));

  RemovePrinter(GetPrinterStore(0), 0);
  EXPECT_EQ(0, GetPrinterCount(0));
}

// Verify that merging data added before sync works.
IN_PROC_BROWSER_TEST_F(SingleClientPrintersSyncTest, AddBeforeSetup) {
  ASSERT_TRUE(SetupClients());

  AddPrinter(GetPrinterStore(0), printers_helper::CreateTestPrinter(0));
  EXPECT_EQ(1, GetPrinterCount(0));

  EXPECT_TRUE(SetupSync()) << "SetupSync() failed.";
}

// Verify that adding a print server printer retains the print server URI.
IN_PROC_BROWSER_TEST_F(SingleClientPrintersSyncTest, AddPrintServerPrinter) {
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
