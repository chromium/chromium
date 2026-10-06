// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/suggestion_service.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/selection/features.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace selection {
namespace {

using ::base::test::TestFuture;
using ::base::test::TestFutureMode;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;

auto SuggestionWithLabel(std::u16string_view label) {
  return Pointee(Property(&Suggestion::GetLabel, label));
}

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
  explicit CustomTestTool(
      std::u16string label = u"Custom Action",
      ToolId tool_id =
          optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
      bool supports_server_suggestions = true)
      : label_(std::move(label)),
        tool_id_(tool_id),
        supports_server_suggestions_(supports_server_suggestions) {}
  ~CustomTestTool() override = default;

  ToolId GetToolId() const override { return tool_id_; }

  void RequestSuggestions(const AreaOfInterest& processed_area,
                          SuggestionsCallback callback) override {
    std::vector<std::unique_ptr<Suggestion>> suggestions;
    suggestions.push_back(std::make_unique<TestSuggestion>(label_));
    std::move(callback).Run(std::move(suggestions), /*complete=*/true);
  }

  bool SupportsServerSuggestions() const override {
    return supports_server_suggestions_;
  }

  std::unique_ptr<Suggestion> CreateSuggestion(
      const AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) override {
    if (server_suggestion.label().empty()) {
      return nullptr;
    }
    return std::make_unique<TestSuggestion>(
        base::UTF8ToUTF16(server_suggestion.label()));
  }

 private:
  const std::u16string label_;
  const ToolId tool_id_;
  const bool supports_server_suggestions_;
};

class AsyncCustomTestTool : public SuggestionTool {
 public:
  AsyncCustomTestTool() = default;
  ~AsyncCustomTestTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_SEARCH;
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

 protected:
  tabs::MockTabInterface& mock_tab() { return mock_tab_; }
  optimization_guide::MockRemoteModelExecutor& mock_model_executor() {
    return mock_model_executor_;
  }
  SuggestionService& service() { return service_; }

 private:
  base::test::TaskEnvironment task_environment_;
  tabs::MockTabInterface mock_tab_;
  optimization_guide::MockRemoteModelExecutor mock_model_executor_;
  SuggestionService service_{&mock_tab_, &mock_model_executor_};
};

// Tests that registered tools provide suggestions and unregistering them works.
TEST_F(SuggestionServiceUnitTest, RegisterAndUnregisterCustomTool) {
  EXPECT_EQ(SuggestionService::From(&mock_tab()), &service());

  CustomTestTool tool1(
      u"Static Action 1",
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  CustomTestTool tool2(
      u"Static Action 2",
      optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS);
  service().RegisterTool(&tool1);
  service().RegisterTool(&tool2);

  AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<Suggestion>>, bool> future{
      TestFutureMode::kQueue};
  service().RequestSuggestions(aoi, future.GetRepeatingCallback());

  // Both synchronous tools are batched into a single callback invocation
  // with complete=true.
  auto [batch, complete] = future.Take();
  EXPECT_TRUE(complete);
  EXPECT_THAT(batch, ElementsAre(SuggestionWithLabel(u"Static Action 1"),
                                 SuggestionWithLabel(u"Static Action 2")));

  service().UnregisterTool(&tool1);
  service().UnregisterTool(&tool2);

  service().RequestSuggestions(aoi, future.GetRepeatingCallback());
  auto [empty_batch, empty_complete] = future.Take();
  EXPECT_TRUE(empty_complete);
  EXPECT_THAT(empty_batch, IsEmpty());
}

// Tests that asynchronous tools return their suggestions in subsequent batches.
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsWithAsyncTool) {
  CustomTestTool static_tool(u"Static Action");
  AsyncCustomTestTool async_tool;
  service().RegisterTool(&static_tool);
  service().RegisterTool(&async_tool);

  AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<Suggestion>>, bool> future{
      TestFutureMode::kQueue};
  service().RequestSuggestions(aoi, future.GetRepeatingCallback());

  // First batch: Synchronous tool suggestions.
  auto [static_suggestions, static_complete] = future.Take();
  EXPECT_FALSE(static_complete);
  EXPECT_THAT(static_suggestions,
              ElementsAre(SuggestionWithLabel(u"Static Action")));

  // Second batch: Async custom tool suggestions.
  auto [async_suggestions, async_complete] = future.Take();
  EXPECT_TRUE(async_complete);
  EXPECT_THAT(async_suggestions,
              ElementsAre(SuggestionWithLabel(u"Async Action")));

  service().UnregisterTool(&static_tool);
  service().UnregisterTool(&async_tool);
}

// Tests that we CHECK that one cannot registering two tools with the same id.
TEST_F(SuggestionServiceUnitTest, DuplicateToolRegistrationChecks) {
  CustomTestTool tool1(
      u"Static Action 1",
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  CustomTestTool tool2(
      u"Static Action 2",
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service().RegisterTool(&tool1);
  EXPECT_CHECK_DEATH(service().RegisterTool(&tool2));
  service().UnregisterTool(&tool1);
}

// Tests that we CHECK that one cannot register a tool with an unspecified id.
TEST_F(SuggestionServiceUnitTest, UnspecifiedToolRegistrationChecks) {
  CustomTestTool unspecified_tool(
      u"Unspecified Action",
      optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED);
  EXPECT_CHECK_DEATH(service().RegisterTool(&unspecified_tool));
}

// Tests that server suggestions are not requested when the feature is disabled.
TEST_F(SuggestionServiceUnitTest, ServerSuggestionsDisabledByDefault) {
  CustomTestTool static_tool(u"Static Action");
  service().RegisterTool(&static_tool);

  EXPECT_CALL(mock_model_executor(), ExecuteModel).Times(0);

  AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<Suggestion>>, bool> future{
      TestFutureMode::kQueue};
  service().RequestSuggestions(aoi, future.GetRepeatingCallback());

  auto [batch, complete] = future.Take();
  EXPECT_TRUE(complete);
  EXPECT_THAT(batch, ElementsAre(SuggestionWithLabel(u"Static Action")));

  service().UnregisterTool(&static_tool);
}

// Tests that server suggestions are not requested when no registered tool
// supports them.
TEST_F(SuggestionServiceUnitTest,
       ServerSuggestionsNotRequestedWhenNoToolSupportsThem) {
  base::test::ScopedFeatureList feature_list{kSmartSelectionServerSuggestions};

  CustomTestTool local_only_tool(
      u"Static Action",
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
      /*supports_server_suggestions=*/false);
  service().RegisterTool(&local_only_tool);

  EXPECT_CALL(mock_model_executor(), ExecuteModel).Times(0);

  AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<Suggestion>>, bool> future{
      TestFutureMode::kQueue};
  service().RequestSuggestions(aoi, future.GetRepeatingCallback());

  auto [batch, complete] = future.Take();
  EXPECT_TRUE(complete);
  EXPECT_THAT(batch, ElementsAre(SuggestionWithLabel(u"Static Action")));

  service().UnregisterTool(&local_only_tool);
}

// Tests that server suggestions are requested, parsed, and returned.
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsWithServerSuggestions) {
  base::test::ScopedFeatureList feature_list{kSmartSelectionServerSuggestions};

  CustomTestTool gemini_tool(
      u"Static Action",
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  CustomTestTool local_only_tool(
      u"Local Only Action",
      optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_SEARCH,
      /*supports_server_suggestions=*/false);
  service().RegisterTool(&gemini_tool);
  service().RegisterTool(&local_only_tool);

  optimization_guide::proto::SmartSelectionSuggestionsResponse response;
  optimization_guide::proto::SmartSelectionSuggestion* s1 =
      response.add_suggestions();
  s1->set_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  s1->set_label("Server Gemini Action");

  optimization_guide::proto::SmartSelectionSuggestion* s2 =
      response.add_suggestions();
  s2->set_tool(optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS);
  s2->set_label("Unregistered Tool Action");

  optimization_guide::proto::SmartSelectionSuggestion* s3 =
      response.add_suggestions();
  s3->set_tool(optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_SEARCH);
  s3->set_label("Local Only Tool Action");

  optimization_guide::proto::SmartSelectionSuggestion* s4 =
      response.add_suggestions();
  s4->set_tool(optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED);
  s4->set_label("Unspecified Tool Action");

  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                               kSmartSelectionSuggestions,
                           _, _, _))
      .WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            EXPECT_EQ(options.execution_timeout, base::Seconds(20));
            const auto& request =
                static_cast<const optimization_guide::proto::
                                SmartSelectionSuggestionsRequest&>(
                    request_metadata);
            EXPECT_THAT(request.client_capabilities().available_tools(),
                        ElementsAre(Property(
                            &optimization_guide::proto::
                                SmartSelectionToolWithCapabilities::tool,
                            optimization_guide::proto::
                                SMART_SELECTION_TOOL_GEMINI_IN_CHROME)));
            ASSERT_EQ(request.areas_of_interest().size(), 1);
            const optimization_guide::proto::AreaOfInterest& proto_aoi =
                request.areas_of_interest(0);
            EXPECT_FALSE(proto_aoi.image_bytes().empty());
            EXPECT_EQ(proto_aoi.mime_type(), "image/png");
            EXPECT_EQ(proto_aoi.selection().x(), 10);
            EXPECT_EQ(proto_aoi.selection().y(), 20);
            EXPECT_EQ(proto_aoi.selection().width(), 30);
            EXPECT_EQ(proto_aoi.selection().height(), 40);
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::ok(optimization_guide::AnyWrapProto(response)),
                    /*execution_info=*/nullptr),
                /*log_entry=*/nullptr);
          });

  AreaOfInterest aoi;
  aoi.screenshot.allocN32Pixels(50, 50);
  aoi.screenshot.eraseColor(SK_ColorRED);
  aoi.bounds = gfx::Rect(10, 20, 30, 40);

  TestFuture<std::vector<std::unique_ptr<Suggestion>>, bool> future{
      TestFutureMode::kQueue};
  service().RequestSuggestions(aoi, future.GetRepeatingCallback());

  // First batch: Synchronous tool suggestions, with complete=false while
  // waiting for MES.
  auto [static_batch, static_complete] = future.Take();
  EXPECT_FALSE(static_complete);
  EXPECT_THAT(static_batch,
              ElementsAre(SuggestionWithLabel(u"Static Action"),
                          SuggestionWithLabel(u"Local Only Action")));

  // Second batch: Server suggestions, with complete=true.
  auto [server_batch, server_complete] = future.Take();
  EXPECT_TRUE(server_complete);
  EXPECT_THAT(server_batch,
              ElementsAre(SuggestionWithLabel(u"Server Gemini Action")));

  service().UnregisterTool(&gemini_tool);
  service().UnregisterTool(&local_only_tool);
}

// Tests that server errors still complete the suggestion request.
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsServerError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kSmartSelectionServerSuggestions,
      {{kSmartSelectionServerTimeout.name, "5s"}});

  CustomTestTool gemini_tool(
      u"Static Action",
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service().RegisterTool(&gemini_tool);

  EXPECT_CALL(
      mock_model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::
              kSmartSelectionSuggestions,
          _,
          Field(&optimization_guide::ModelExecutionOptions::execution_timeout,
                base::Seconds(5)),
          _))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              base::unexpected(
                  optimization_guide::OptimizationGuideModelExecutionError::
                      FromModelExecutionError(
                          optimization_guide::
                              OptimizationGuideModelExecutionError::
                                  ModelExecutionError::kGenericFailure)),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<Suggestion>>, bool> future{
      TestFutureMode::kQueue};
  service().RequestSuggestions(aoi, future.GetRepeatingCallback());

  auto [static_batch, static_complete] = future.Take();
  EXPECT_FALSE(static_complete);
  EXPECT_THAT(static_batch, ElementsAre(SuggestionWithLabel(u"Static Action")));

  auto [server_batch, server_complete] = future.Take();
  EXPECT_TRUE(server_complete);
  EXPECT_THAT(server_batch, IsEmpty());

  service().UnregisterTool(&gemini_tool);
}

}  // namespace
}  // namespace selection
