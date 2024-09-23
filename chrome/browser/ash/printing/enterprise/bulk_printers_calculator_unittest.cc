// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/printing/printer_configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// The number of correct printers in BulkPolicyContentsJson.
constexpr size_t kNumValidPrinters = 3;

// An example bulk printer configuration file.
constexpr char kBulkPolicyContentsJson[] = R"json(
[
  {
    "guid": "First",
    "display_name": "LexaPrint",
    "description": "Laser on the test shelf",
    "uri": "ipp://192.168.1.5",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }, {
    "guid": "Incorrect uri",
    "display_name": "aaa",
    "description": "bbbb",
    "uri":"ipp://:",
    "ppd_resource":{
      "effective_model": "fff"
    }
  }, {
    "guid": "Second",
    "display_name": "Color Laser",
    "uri":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
    "ppd_resource":{
      "effective_model": "ColorLaser2k4"
    }
  }, {
    "guid": "Third",
    "display_name": "YaLP",
    "description": "Fancy Fancy Fancy",
    "uri": "ipp://192.168.1.8",
    "ppd_resource": {
      "autoconf": true
    }
  }
])json";

// A different bulk printer configuration file.
constexpr char kMoreContentsJson[] = R"json(
[
  {
    "guid": "ThirdPrime",
    "display_name": "Printy McPrinter",
    "description": "Laser on the test shelf",
    "uri": "ipp://192.168.1.5",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }
])json";

// Observer that counts the number of times it has been called.
class TestObserver : public BulkPrintersCalculator::Observer {
 public:
  void OnPrintersChanged(const BulkPrintersCalculator* sender) override {
    last_valid = sender->IsComplete();
    called++;
  }

  // Counts the number of times the observer is invoked.
  int called = 0;
  // Holds the most recent value of valid.
  bool last_valid = false;
};

class BulkPrintersCalculatorTest : public testing::Test {
 public:
  BulkPrintersCalculatorTest() : task_environment_() {
    external_printers_ = BulkPrintersCalculator::Create();
  }
  ~BulkPrintersCalculatorTest() override {
    // Delete the printer before the task environment.
    external_printers_.reset();
  }

 protected:
  std::unique_ptr<BulkPrintersCalculator> external_printers_;
  base::test::TaskEnvironment task_environment_;
};

// Verify that we're initiall unset and empty.
TEST_F(BulkPrintersCalculatorTest, InitialConditions) {
  EXPECT_FALSE(external_printers_->IsDataPolicySet());
  EXPECT_TRUE(external_printers_->GetPrinters().empty());
}

// Verify that the object can be destroyed while parsing is in progress.
TEST_F(BulkPrintersCalculatorTest, DestructionIsSafe) {
  {
    std::unique_ptr<BulkPrintersCalculator> printers =
        BulkPrintersCalculator::Create();
    printers->SetAccessMode(BulkPrintersCalculator::BLOCKLIST_ONLY);
    printers->SetBlocklist({"Third"});
    printers->SetData(std::make_unique<std::string>(kBulkPolicyContentsJson));
    // Data is valid.  Computation is proceeding.
  }
  // printers is out of scope.  Destructor has run.  Pump the message queue to
  // see if anything strange happens.
  task_environment_.RunUntilIdle();
}

// Verifies that IsDataPolicySet returns false until data is set.
TEST_F(BulkPrintersCalculatorTest, PolicyUnsetWithMissingData) {
  auto data = std::make_unique<std::string>(kBulkPolicyContentsJson);
  external_printers_->ClearData();
  EXPECT_FALSE(external_printers_->IsDataPolicySet());
  external_printers_->SetData(std::move(data));
  EXPECT_TRUE(external_printers_->IsDataPolicySet());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(external_printers_->IsComplete());
}

// Verify printer list after all attributes have been set.
TEST_F(BulkPrintersCalculatorTest, AllPoliciesResultInPrinters) {
  auto data = std::make_unique<std::string>(kBulkPolicyContentsJson);
  external_printers_->SetAccessMode(
      BulkPrintersCalculator::AccessMode::ALL_ACCESS);
  external_printers_->SetData(std::move(data));
  EXPECT_TRUE(external_printers_->IsDataPolicySet());

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(external_printers_->IsComplete());
  const auto& printers = external_printers_->GetPrinters();
  EXPECT_EQ(kNumValidPrinters, printers.size());
  EXPECT_EQ("LexaPrint", printers.at("First").display_name());
  EXPECT_EQ("Color Laser", printers.at("Second").display_name());
  EXPECT_EQ("YaLP", printers.at("Third").display_name());
}

// The external policy was cleared, results should be invalidated.
TEST_F(BulkPrintersCalculatorTest, PolicyClearedNowUnset) {
  auto data = std::make_unique<std::string>(kBulkPolicyContentsJson);
  external_printers_->SetAccessMode(
      BulkPrintersCalculator::AccessMode::ALL_ACCESS);
  external_printers_->ClearData();
  external_printers_->SetData(std::move(data));

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(external_printers_->IsDataPolicySet());

  external_printers_->ClearData();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(external_printers_->IsDataPolicySet());
  EXPECT_TRUE(external_printers_->GetPrinters().empty());
}

// Verify that the blocklist policy is applied correctly.  Printers in the
// blocklist policy should not be available.  Printers not in the blocklist
// should be available.
TEST_F(BulkPrintersCalculatorTest, BlocklistPolicySet) {
  auto data = std::make_unique<std::string>(kBulkPolicyContentsJson);
  external_printers_->ClearData();
  external_printers_->SetData(std::move(data));
  external_printers_->SetAccessMode(BulkPrintersCalculator::BLOCKLIST_ONLY);
  task_environment_.RunUntilIdle();
  external_printers_->SetBlocklist({"Second", "Third"});
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(external_printers_->IsComplete());

  task_environment_.RunUntilIdle();
  const auto& printers = external_printers_->GetPrinters();
  EXPECT_EQ(1U, printers.size());
  EXPECT_EQ("LexaPrint", printers.at("First").display_name());
}

// Verify that the allowlist policy is correctly applied.  Only printers
// available in the allowlist are available.
TEST_F(BulkPrintersCalculatorTest, AllowlistPolicySet) {
  auto data = std::make_unique<std::string>(kBulkPolicyContentsJson);
  external_printers_->ClearData();
  external_printers_->SetData(std::move(data));
  external_printers_->SetAccessMode(BulkPrintersCalculator::ALLOWLIST_ONLY);
  task_environment_.RunUntilIdle();
  external_printers_->SetAllowlist({"First"});

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(external_printers_->IsComplete());
  const auto& printers = external_printers_->GetPrinters();
  EXPECT_EQ(1U, printers.size());
  EXPECT_EQ("LexaPrint", printers.at("First").display_name());
}

// Verify that an empty blocklist results in no printer limits.
TEST_F(BulkPrintersCalculatorTest, EmptyBlocklistAllPrinters) {
  auto data = std::make_unique<std::string>(kBulkPolicyContentsJson);
  external_printers_->ClearData();
  external_printers_->SetData(std::move(data));
  external_printers_->SetAccessMode(BulkPrintersCalculator::BLOCKLIST_ONLY);
  task_environment_.RunUntilIdle();
  external_printers_->SetBlocklist({});

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(external_printers_->IsComplete());
  const auto& printers = external_printers_->GetPrinters();
  EXPECT_EQ(kNumValidPrinters, printers.size());
}

// Verify that an empty allowlist results in no printers.
TEST_F(BulkPrintersCalculatorTest, EmptyAllowlistNoPrinters) {
  auto data = std::make_unique<std::string>(kBulkPolicyContentsJson);
  external_printers_->ClearData();
  external_printers_->SetData(std::move(data));
  external_printers_->SetAccessMode(BulkPrintersCalculator::ALLOWLIST_ONLY);
  task_environment_.RunUntilIdle();
  external_printers_->SetAllowlist({});

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(external_printers_->IsComplete());
  const auto& printers = external_printers_->GetPrinters();
  EXPECT_EQ(0U, printers.size());
}

// Verify that switching from allowlist to blocklist behaves correctly.
TEST_F(BulkPrintersCalculatorTest, BlocklistToAllowlistSwap) {
  auto data = std::make_unique<std::string>(kBulkPolicyContentsJson);
  external_printers_->ClearData();
  external_printers_->SetData(std::move(data));
  external_printers_->SetAccessMode(BulkPrintersCalculator::BLOCKLIST_ONLY);
  external_printers_->SetAllowlist({"First"});
  external_printers_->SetBlocklist({"First"});

  // This should result in 2 printers.  But we're switching the mode anyway.

  external_printers_->SetAccessMode(BulkPrintersCalculator::ALLOWLIST_ONLY);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(external_printers_->IsComplete());
  const auto& printers = external_printers_->GetPrinters();
  EXPECT_EQ(1U, printers.size());
  EXPECT_EQ("LexaPrint", printers.at("First").display_name());
}

// Verify that updated configurations are handled properly.
TEST_F(BulkPrintersCalculatorTest, MultipleUpdates) {
  auto data = std::make_unique<std::string>(kBulkPolicyContentsJson);
  external_printers_->ClearData();
  external_printers_->SetData(std::move(data));
  external_printers_->SetAccessMode(BulkPrintersCalculator::ALL_ACCESS);
  // There will be 3 printers here.  But we don't want to wait for computation
  // to complete to verify the final value gets used.

  auto new_data = std::make_unique<std::string>(kMoreContentsJson);
  external_printers_->SetData(std::move(new_data));
  task_environment_.RunUntilIdle();
  const auto& printers = external_printers_->GetPrinters();
  ASSERT_EQ(1U, printers.size());
  EXPECT_EQ("ThirdPrime", printers.at("ThirdPrime").id());
}

// Verifies that the observer is called at the expected times.
TEST_F(BulkPrintersCalculatorTest, ObserverTest) {
  TestObserver obs;
  external_printers_->AddObserver(&obs);

  external_printers_->SetAccessMode(BulkPrintersCalculator::ALL_ACCESS);
  external_printers_->SetAllowlist(std::vector<std::string>());
  external_printers_->SetBlocklist(std::vector<std::string>());
  external_printers_->ClearData();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, obs.called);

  external_printers_->SetData(
      std::make_unique<std::string>(kBulkPolicyContentsJson));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(external_printers_->IsDataPolicySet());
  EXPECT_EQ(2, obs.called);
  EXPECT_TRUE(obs.last_valid);  // ready now
  // Printer list is correct after notification.
  EXPECT_EQ(kNumValidPrinters, external_printers_->GetPrinters().size());

  external_printers_->SetAccessMode(BulkPrintersCalculator::ALLOWLIST_ONLY);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(3, obs.called);  // effective list changed.  Notified.
  EXPECT_TRUE(obs.last_valid);

  external_printers_->SetAccessMode(BulkPrintersCalculator::BLOCKLIST_ONLY);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(4, obs.called);  // effective list changed.  Notified.
  EXPECT_TRUE(obs.last_valid);

  external_printers_->ClearData();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(5, obs.called);  // Called for transition to invalid policy.
  EXPECT_TRUE(obs.last_valid);
  EXPECT_TRUE(external_printers_->GetPrinters().empty());

  // cleanup
  external_printers_->RemoveObserver(&obs);
}

}  // namespace
}  // namespace ash
