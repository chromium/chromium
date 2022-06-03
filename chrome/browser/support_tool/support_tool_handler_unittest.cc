// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_handler.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "testing/gtest/include/gtest/gtest.h"

// TestDataCollector implements DataCollector functions for testing.
class TestDataCollector : public DataCollector {
 public:
  explicit TestDataCollector(std::string name) : DataCollector(), name_(name) {}
  ~TestDataCollector() override = default;

  // Overrides from DataCollector.
  std::string GetName() const override { return name_; }

  std::string GetDescription() const override {
    return "The data collector that will be used for testing";
  }

  const PIIMap& GetDetectedPII() override { return pii_map_; }

  void CollectDataAndDetectPII(
      base::OnceCallback<void()> on_data_collected_callback) override {
    // Add fake PII for testing.
    AddPIIForTesting();
    std::move(on_data_collected_callback).Run();
  }

 private:
  // Adds an entry to the PIIMap with the name of the data collector. The
  // PIIType is not significant at this point so we use a random one.
  void AddPIIForTesting() {
    pii_map_.insert(std::pair<PIIType, std::string>(
        PIIType::kUIHierarchyWindowTitles, name_));
  }

  // Name of the TestDataCollector. It will be used to create fake PII inside
  // the PIIMap.
  std::string name_;
  PIIMap pii_map_;
};

class SupportToolHandlerTest : public ::testing::Test {
 public:
  SupportToolHandlerTest() {
    // Create and set up SupportToolHandler.
    handler_ = std::make_unique<SupportToolHandler>();
    SetUpHandler();
  }

  SupportToolHandlerTest(const SupportToolHandlerTest&) = delete;
  SupportToolHandlerTest& operator=(const SupportToolHandlerTest&) = delete;

  // Run SupportToolHandler's CollectSupportData and returns the result.
  const PIIMap& CollectData() {
    base::RunLoop run_loop;
    handler_->CollectSupportData(
        base::BindOnce(&SupportToolHandlerTest::OnDataCollected,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return pii_result_;
  }

 private:
  // Adds TestDataCollectors to SupportToolHandler.
  void SetUpHandler() {
    handler_->AddDataCollector(
        std::make_unique<TestDataCollector>("test_data_collector_1"));
    handler_->AddDataCollector(
        std::make_unique<TestDataCollector>("test_data_collector_2"));
  }

  void OnDataCollected(base::OnceClosure quit_closure, const PIIMap& result) {
    pii_result_ = result;
    std::move(quit_closure).Run();
  }

  std::unique_ptr<SupportToolHandler> handler_;
  PIIMap pii_result_;
};

TEST_F(SupportToolHandlerTest, CollectSupportData) {
  base::test::TaskEnvironment task_environment;

  const PIIMap& detected_pii = CollectData();
  // Check if the detected PII map returned from the SupportToolHandler is
  // empty.
  EXPECT_FALSE(detected_pii.empty());
}
