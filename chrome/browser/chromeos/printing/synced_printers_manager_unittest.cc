// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/synced_printers_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/printing/printers_sync_bridge.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/model/fake_model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kTestPrinterId[] = "UUID-UUID-UUID-PRINTER";
constexpr char kTestPrinterId2[] = "UUID-UUID-UUID-PRINTR2";
constexpr char kTestUri[] = "ipps://printer.chromium.org/ipp/print";

constexpr char kLexJson[] = R"({
        "display_name": "LexaPrint",
        "description": "Laser on the test shelf",
        "manufacturer": "LexaPrint, Inc.",
        "model": "MS610de",
        "uri": "ipp://192.168.1.5",
        "ppd_resource": {
          "effective_manufacturer": "LexaPrint",
          "effective_model": "MS610de",
        },
      } )";

constexpr char kColorLaserJson[] = R"json({
      "display_name": "Color Laser",
      "description": "The printer next to the water cooler.",
      "manufacturer": "Printer Manufacturer",
      "model":"Color Laser 2004",
      "uri":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
      "uuid":"1c395fdb-5d93-4904-b246-b2c046e79d12",
      "ppd_resource":{
          "effective_manufacturer": "MakesPrinters",
          "effective_model":"ColorLaser2k4"
       }
      })json";

// Helper class to record observed events.
class LoggingObserver : public SyncedPrintersManager::Observer {
 public:
  explicit LoggingObserver(SyncedPrintersManager* source)
      : observer_(this), manager_(source) {
    observer_.Add(source);
  }

  void OnSavedPrintersChanged() override {
    saved_printers_ = manager_->GetSavedPrinters();
  }

  void OnEnterprisePrintersChanged() override {
    manager_->GetEnterprisePrinters(&enterprise_printers_);
  }

  const std::vector<Printer>& saved_printers() const { return saved_printers_; }
  const std::vector<Printer>& enterprise_printers() const {
    return enterprise_printers_;
  }

 private:
  std::vector<Printer> saved_printers_;
  std::vector<Printer> enterprise_printers_;
  ScopedObserver<SyncedPrintersManager, SyncedPrintersManager::Observer>
      observer_;
  SyncedPrintersManager* manager_;
};

class SyncedPrintersManagerTest : public testing::Test {
 protected:
  SyncedPrintersManagerTest()
      : manager_(SyncedPrintersManager::Create(
            &profile_,
            std::make_unique<PrintersSyncBridge>(
                syncer::ModelTypeStoreTestUtil::
                    FactoryForInMemoryStoreForTest(),
                base::BindRepeating(
                    base::IgnoreResult(&base::debug::DumpWithoutCrashing))))) {
    base::RunLoop().RunUntilIdle();
  }

  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;

  // Must outlive |manager_|.
  TestingProfile profile_;

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
  updated_printer.set_uri(kTestUri);

  // Register observer so it only receives the update event.
  LoggingObserver observer(manager_.get());

  manager_->UpdateSavedPrinter(updated_printer);

  auto printers = manager_->GetSavedPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ(kTestUri, printers[0].uri());

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

// Tests for policy printers

TEST_F(SyncedPrintersManagerTest, EnterprisePrinters) {
  std::string first_printer = kColorLaserJson;
  std::string second_printer = kLexJson;

  auto value = std::make_unique<base::ListValue>();
  value->AppendString(first_printer);
  value->AppendString(second_printer);

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_.GetTestingPrefService();
  // TestingPrefSyncableService assumes ownership of |value|.
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, std::move(value));

  std::vector<Printer> printers;
  manager_->GetEnterprisePrinters(&printers);
  ASSERT_EQ(2U, printers.size());
  // order not specified
  // EXPECT_EQ("Color Laser", printers[0].display_name());
  // EXPECT_EQ("ipp://192.168.1.5", printers[1].uri());
  EXPECT_EQ(Printer::Source::SRC_POLICY, printers[1].source());
}

TEST_F(SyncedPrintersManagerTest, GetEnterprisePrinter) {
  std::string printer = kLexJson;
  auto value = std::make_unique<base::ListValue>();
  value->AppendString(printer);

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_.GetTestingPrefService();
  // TestingPrefSyncableService assumes ownership of |value|.
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, std::move(value));

  std::vector<Printer> printers;
  manager_->GetEnterprisePrinters(&printers);

  const Printer& from_list = printers.front();
  std::unique_ptr<Printer> retrieved = manager_->GetPrinter(from_list.id());

  EXPECT_EQ(from_list.id(), retrieved->id());
  EXPECT_EQ("LexaPrint", from_list.display_name());
  EXPECT_EQ(Printer::Source::SRC_POLICY, from_list.source());
}

// Test that UpdateSavedPrinter saves a printer if it doesn't appear in the
// enterprise or saved printer lists.
TEST_F(SyncedPrintersManagerTest, UpdateSavedPrinterSavesPrinter) {
  // Set up an enterprise printer.
  auto value = std::make_unique<base::ListValue>();
  value->AppendString(kColorLaserJson);

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile_.GetTestingPrefService();
  prefs->SetManagedPref(prefs::kRecommendedNativePrinters, std::move(value));

  // Figure out the id of the enterprise printer that was just installed.
  std::vector<Printer> printers;
  manager_->GetEnterprisePrinters(&printers);
  std::string enterprise_id = printers.at(0).id();

  Printer saved(kTestPrinterId);

  // Install |saved| printer.
  manager_->UpdateSavedPrinter(saved);
  auto found_printer = manager_->GetPrinter(kTestPrinterId);
  ASSERT_TRUE(found_printer);
  EXPECT_TRUE(found_printer->display_name().empty());

  // Saving the enterprise printer should *not* generate a configuration
  // update.
  manager_->UpdateSavedPrinter(Printer(enterprise_id));
  EXPECT_EQ(1U, manager_->GetSavedPrinters().size());

  // Saving a printer we don't know about *should* generate a configuration
  // update.
  manager_->UpdateSavedPrinter(Printer(kTestPrinterId2));
  EXPECT_EQ(2U, manager_->GetSavedPrinters().size());
}

}  // namespace
}  // namespace chromeos
