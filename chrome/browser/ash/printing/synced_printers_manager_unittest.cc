// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/synced_printers_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/printing/printers_sync_bridge.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::chromeos::Printer;

constexpr char kTestPrinterId[] = "UUID-UUID-UUID-PRINTER";
constexpr char kTestPrinterId2[] = "UUID-UUID-UUID-PRINTR2";
constexpr char kTestUri[] = "ipps://printer.chromium.org/ipp/print";

// Helper class to record observed events.
class LoggingObserver : public SyncedPrintersManager::Observer {
 public:
  explicit LoggingObserver(SyncedPrintersManager* source) : manager_(source) {
    observation_.Observe(source);
  }

  void OnSavedPrintersChanged() override {
    saved_printers_ = manager_->GetSavedPrinters();
  }

  const std::vector<Printer>& saved_printers() const { return saved_printers_; }

 private:
  std::vector<Printer> saved_printers_;
  base::ScopedObservation<SyncedPrintersManager,
                          SyncedPrintersManager::Observer>
      observation_{this};
  raw_ptr<SyncedPrintersManager> manager_;
};

class SyncedPrintersManagerTest : public testing::Test {
 protected:
  SyncedPrintersManagerTest()
      : manager_(
            SyncedPrintersManager::Create(std::make_unique<PrintersSyncBridge>(
                syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
                base::BindRepeating(
                    base::IgnoreResult(&base::debug::DumpWithoutCrashing),
                    FROM_HERE,
                    base::Minutes(5))))) {
    base::RunLoop().RunUntilIdle();
  }

  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<SyncedPrintersManager> manager_;
};

// Add a test failure if the ids of printers are not those in expected.  Order
// is not considered.
void ExpectObservedPrinterIdsAre(const std::vector<Printer>& printers,
                                 std::vector<std::string> expected_ids) {
  // Ensure all callbacks have completed before we check.
  base::RunLoop().RunUntilIdle();

  std::sort(expected_ids.begin(), expected_ids.end());
  std::vector<std::string> printer_ids;
  for (const Printer& printer : printers) {
    printer_ids.push_back(printer.id());
  }
  std::sort(printer_ids.begin(), printer_ids.end());
  if (printer_ids != expected_ids) {
    ADD_FAILURE() << "Expected to find ids: {"
                  << base::JoinString(expected_ids, ",") << "}; found ids: {"
                  << base::JoinString(printer_ids, ",") << "}";
  }
}

TEST_F(SyncedPrintersManagerTest, AddPrinter) {
  LoggingObserver observer(manager_.get());
  manager_->UpdateSavedPrinter(Printer(kTestPrinterId));

  auto printers = manager_->GetSavedPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kTestPrinterId, printers[0].id());
  EXPECT_EQ(Printer::Source::SRC_USER_PREFS, printers[0].source());

  ExpectObservedPrinterIdsAre(observer.saved_printers(), {kTestPrinterId});
}

TEST_F(SyncedPrintersManagerTest, UpdatePrinterAssignsId) {
  manager_->UpdateSavedPrinter(Printer());
  auto printers = manager_->GetSavedPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_FALSE(printers[0].id().empty());
}

TEST_F(SyncedPrintersManagerTest, UpdatePrinter) {
  manager_->UpdateSavedPrinter(Printer(kTestPrinterId));
  Printer updated_printer(kTestPrinterId);
  updated_printer.SetUri(kTestUri);

  // Register observer so it only receives the update event.
  LoggingObserver observer(manager_.get());

  manager_->UpdateSavedPrinter(updated_printer);

  auto printers = manager_->GetSavedPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kTestUri, printers[0].uri().GetNormalized(false));

  ExpectObservedPrinterIdsAre(observer.saved_printers(), {kTestPrinterId});
}

TEST_F(SyncedPrintersManagerTest, RemovePrinter) {
  manager_->UpdateSavedPrinter(Printer("OtherUUID"));
  manager_->UpdateSavedPrinter(Printer(kTestPrinterId));
  manager_->UpdateSavedPrinter(Printer());

  manager_->RemoveSavedPrinter(kTestPrinterId);

  auto printers = manager_->GetSavedPrinters();

  // One of the remaining ids should be "OtherUUID", the other should have
  // been automatically generated by the manager.
  ASSERT_EQ(2U, printers.size());
  EXPECT_NE(kTestPrinterId, printers.at(0).id());
  EXPECT_NE(kTestPrinterId, printers.at(1).id());
}

// Test that UpdateSavedPrinter saves a printer if it doesn't appear in the
// saved printer lists.
TEST_F(SyncedPrintersManagerTest, UpdateSavedPrinterSavesPrinter) {
  Printer saved(kTestPrinterId);

  // Install |saved| printer.
  manager_->UpdateSavedPrinter(saved);
  auto found_printer = manager_->GetPrinter(kTestPrinterId);
  ASSERT_TRUE(found_printer);
  EXPECT_TRUE(found_printer->display_name().empty());

  // Saving a printer we know about *should not* generate a configuration
  // update.
  manager_->UpdateSavedPrinter(*found_printer);
  EXPECT_EQ(1U, manager_->GetSavedPrinters().size());

  // Saving a printer we don't know about *should* generate a configuration
  // update.
  manager_->UpdateSavedPrinter(Printer(kTestPrinterId2));
  EXPECT_EQ(2U, manager_->GetSavedPrinters().size());
}

}  // namespace
}  // namespace ash
