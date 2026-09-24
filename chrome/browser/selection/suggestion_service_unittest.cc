// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/suggestion_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace selection {
namespace {

class TestSuggestion : public Suggestion {
 public:
  explicit TestSuggestion(std::u16string label) : label_(std::move(label)) {}
  ~TestSuggestion() override = default;

  // Suggestion:
  const std::u16string& GetLabel() const override { return label_; }
  void OnSuggestionPresented() override {}
  void OnSuggestionExecuted() override {}
  mojom::ActionPtr GetAction() const override {
    return mojom::Action::NewHandoff(mojom::Handoff::New());
  }

 private:
  std::u16string label_;
};

class CustomTestTool : public SuggestionTool {
 public:
  explicit CustomTestTool(std::u16string label = u"Custom Action")
      : label_(std::move(label)) {}
  ~CustomTestTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED;
  }

  void RequestSuggestions(const AreaOfInterest& processed_area,
                          SuggestionsCallback callback) override {
    std::vector<std::unique_ptr<Suggestion>> suggestions;
    suggestions.push_back(std::make_unique<TestSuggestion>(label_));
    std::move(callback).Run(std::move(suggestions), /*complete=*/true);
  }

 private:
  std::u16string label_;
};

class AsyncCustomTestTool : public SuggestionTool {
 public:
  AsyncCustomTestTool() = default;
  ~AsyncCustomTestTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED;
  }

  void RequestSuggestions(const AreaOfInterest& processed_area,
                          SuggestionsCallback callback) override {
    std::vector<std::unique_ptr<Suggestion>> suggestions;
    suggestions.push_back(std::make_unique<TestSuggestion>(u"Async Action"));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(suggestions),
                                  /*complete=*/true));
  }
};

class SuggestionServiceUnitTest : public testing::Test {
 public:
  SuggestionServiceUnitTest() = default;
  ~SuggestionServiceUnitTest() override = default;

  void SetUp() override {
    service_ = std::make_unique<SuggestionService>(&mock_tab_);
  }

  void TearDown() override { service_.reset(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  tabs::MockTabInterface mock_tab_;
  std::unique_ptr<SuggestionService> service_;
};

TEST_F(SuggestionServiceUnitTest, RegisterAndUnregisterCustomTool) {
  EXPECT_EQ(SuggestionService::From(&mock_tab_), service_.get());

  CustomTestTool tool1(u"Static Action 1");
  CustomTestTool tool2(u"Static Action 2");
  service_->RegisterTool(&tool1);
  service_->RegisterTool(&tool2);

  AreaOfInterest aoi;
  base::test::TestFuture<std::vector<std::unique_ptr<Suggestion>>, bool> future{
      base::test::TestFutureMode::kQueue};
  service_->RequestSuggestions(aoi, future.GetRepeatingCallback());

  // Both synchronous tools are batched into a single callback invocation
  // with complete=true.
  auto [batch, complete] = future.Take();
  EXPECT_TRUE(complete);
  ASSERT_EQ(batch.size(), 2u);
  EXPECT_EQ(batch[0]->GetLabel(), u"Static Action 1");
  EXPECT_EQ(batch[1]->GetLabel(), u"Static Action 2");

  service_->UnregisterTool(&tool1);
  service_->UnregisterTool(&tool2);

  service_->RequestSuggestions(aoi, future.GetRepeatingCallback());
  auto [empty_batch, empty_complete] = future.Take();
  EXPECT_TRUE(empty_complete);
  EXPECT_TRUE(empty_batch.empty());
}

TEST_F(SuggestionServiceUnitTest, RequestSuggestionsWithAsyncTool) {
  CustomTestTool static_tool(u"Static Action");
  AsyncCustomTestTool async_tool;
  service_->RegisterTool(&static_tool);
  service_->RegisterTool(&async_tool);

  AreaOfInterest aoi;
  base::test::TestFuture<std::vector<std::unique_ptr<Suggestion>>, bool> future{
      base::test::TestFutureMode::kQueue};
  service_->RequestSuggestions(aoi, future.GetRepeatingCallback());

  // First batch: Synchronous tool suggestions.
  auto [static_suggestions, static_complete] = future.Take();
  EXPECT_FALSE(static_complete);
  ASSERT_EQ(static_suggestions.size(), 1u);
  EXPECT_EQ(static_suggestions[0]->GetLabel(), u"Static Action");

  // Second batch: Async custom tool suggestions.
  auto [async_suggestions, async_complete] = future.Take();
  EXPECT_TRUE(async_complete);
  ASSERT_EQ(async_suggestions.size(), 1u);
  EXPECT_EQ(async_suggestions[0]->GetLabel(), u"Async Action");

  service_->UnregisterTool(&static_tool);
  service_->UnregisterTool(&async_tool);
}

}  // namespace
}  // namespace selection

