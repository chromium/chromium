// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printers_sync_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using sync_pb::PrinterSpecifics;

constexpr char kInkyDescription[] = "InkJetInkJetInkJet";
constexpr char kLazerDescription[] = "LAZERS! Pew Pew";
constexpr char kUUID[] = "DEADBEEFDEADBEEFDEADBEEF";

class PrintersSyncBridgeTest : public testing::Test {
 public:
  PrintersSyncBridgeTest() : task_environment_() {
    bridge_ = std::make_unique<PrintersSyncBridge>(
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        base::BindRepeating(
            base::IgnoreResult(&base::debug::DumpWithoutCrashing), FROM_HERE,
            base::Minutes(5)));
  }

 protected:
  std::unique_ptr<PrintersSyncBridge> bridge_;

 private:
  base::test::TaskEnvironment task_environment_;
};

std::unique_ptr<PrinterSpecifics> TestPrinter(const std::string& id) {
  auto printer = std::make_unique<PrinterSpecifics>();
  printer->set_id(id);

  return printer;
}

// Verifies that AddPrinter overwrites printers that share an id.
TEST_F(PrintersSyncBridgeTest, AddPrinterOverwrites) {
  auto first = TestPrinter("0");
  first->set_description(kInkyDescription);
  first->set_uuid(kUUID);
  bridge_->AddPrinter(std::move(first));

  auto overwrite = TestPrinter("0");
  overwrite->set_description(kLazerDescription);
  bridge_->AddPrinter(std::move(overwrite));

  std::optional<PrinterSpecifics> printer = bridge_->GetPrinter("0");
  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ("0", printer->id());
  EXPECT_EQ(kLazerDescription, printer->description());
  // UUID is expected to be deleted because it was overwritten.
  EXPECT_FALSE(printer->has_uuid());
}

// Verifies that UpdatePrinter merges fields with existing printers.
TEST_F(PrintersSyncBridgeTest, UpdatePrinterMerge) {
  auto first = TestPrinter("0");
  first->set_description(kInkyDescription);
  first->set_uuid(kUUID);
  bridge_->AddPrinter(std::move(first));

  auto overwrite = TestPrinter("0");
  overwrite->set_description(kLazerDescription);
  bool is_new = bridge_->UpdatePrinter(std::move(overwrite));
  EXPECT_FALSE(is_new);

  std::optional<PrinterSpecifics> printer = bridge_->GetPrinter("0");
  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ("0", printer->id());
  // Description is overwritten.
  EXPECT_EQ(kLazerDescription, printer->description());
  // UUID is retained.
  EXPECT_EQ(kUUID, printer->uuid());
}

// Verifies that if an id is new, UpdatePrinter adds a new printer.
TEST_F(PrintersSyncBridgeTest, UpdatePrinterNewPrinter) {
  auto first = TestPrinter("0");
  first->set_description(kInkyDescription);
  first->set_uuid(kUUID);
  bool is_new = bridge_->UpdatePrinter(std::move(first));
  EXPECT_TRUE(is_new);

  std::optional<PrinterSpecifics> printer = bridge_->GetPrinter("0");
  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ("0", printer->id());
  EXPECT_EQ(kInkyDescription, printer->description());
  EXPECT_EQ(kUUID, printer->uuid());
  // Double check that the timestamp gets set.
  EXPECT_TRUE(printer->has_updated_timestamp());
}

}  // namespace
}  // namespace ash
