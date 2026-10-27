// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/suggestion_service.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/selection/features.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/google_groups_manager.h"
#include "components/variations/variations_seed_processor.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"

namespace selection {
namespace {

using ::base::test::RunOnceCallback;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

auto SuggestionWithLabel(std::u16string_view label) {
  return Pointee(Property(&Suggestion::GetLabel, label));
}

auto ToolWithCapabilities(
    optimization_guide::proto::SmartSelectionToolId tool_id,
    auto capabilities_matcher) {
  using ProtoTool =
      optimization_guide::proto::SmartSelectionToolWithCapabilities;
  // `capabilities` is overloaded - this picks the overload that we want.
  using CapabilitiesGetter =
      const google::protobuf::RepeatedPtrField<std::string>& (ProtoTool::*)()
          const;
  return AllOf(
      Property(&ProtoTool::tool, tool_id),
      Property(static_cast<CapabilitiesGetter>(&ProtoTool::capabilities),
               std::move(capabilities_matcher)));
}

class TestSuggestion : public Suggestion {
 public:
  explicit TestSuggestion(std::u16string label) : label_(std::move(label)) {}
  ~TestSuggestion() override = default;

  // Suggestion:
  const std::u16string& GetLabel() const override { return label_; }
  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }
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
      ToolId tool_id =
          optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
      std::vector<std::string> capabilities = {})
      : tool_id_(tool_id), capabilities_(std::move(capabilities)) {}
  ~CustomTestTool() override = default;

  ToolId GetToolId() const override { return tool_id_; }

  std::vector<std::string> GetCapabilities() const override {
    return capabilities_;
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
  const ToolId tool_id_;
  const std::vector<std::string> capabilities_;
};

class SuggestionServiceUnitTest : public testing::Test {
 public:
  SuggestionServiceUnitTest() {
    target_prefs_.registry()->RegisterDictionaryPref(
        variations::prefs::kVariationsGoogleGroups);
    GoogleGroupsManager::RegisterProfilePrefs(source_prefs_.registry());
    google_groups_manager_.emplace(target_prefs_, "Default", source_prefs_);
    service_.emplace(&mock_tab_, &mock_model_executor_,
                     &google_groups_manager_.value());
  }
  ~SuggestionServiceUnitTest() override = default;

 protected:
  tabs::MockTabInterface& mock_tab() { return mock_tab_; }
  optimization_guide::MockRemoteModelExecutor& mock_model_executor() {
    return mock_model_executor_;
  }
  SuggestionService& service() { return *service_; }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple target_prefs_;
  sync_preferences::TestingPrefServiceSyncable source_prefs_;
  std::optional<GoogleGroupsManager> google_groups_manager_;
  tabs::MockTabInterface mock_tab_;
  optimization_guide::MockRemoteModelExecutor mock_model_executor_;
  std::optional<SuggestionService> service_;
};

// Tests that registering and unregistering tools works.
TEST_F(SuggestionServiceUnitTest, RegisterAndUnregisterCustomTool) {
  EXPECT_EQ(SuggestionService::From(&mock_tab()), &service());

  CustomTestTool tool1(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  CustomTestTool tool2(
      optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS);
  service().RegisterTool(&tool1);
  service().RegisterTool(&tool2);

  service().UnregisterTool(&tool1);
  service().UnregisterTool(&tool2);

  // Re-registering with the same ToolId succeeds after unregistering.
  service().RegisterTool(&tool1);
  service().UnregisterTool(&tool1);
}

// Tests that we CHECK that one cannot registering two tools with the same id.
TEST_F(SuggestionServiceUnitTest, DuplicateToolRegistrationChecks) {
  CustomTestTool tool1(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  CustomTestTool tool2(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service().RegisterTool(&tool1);
  EXPECT_CHECK_DEATH(service().RegisterTool(&tool2));
  service().UnregisterTool(&tool1);
}

// Tests that we CHECK that one cannot register a tool with an unspecified id.
TEST_F(SuggestionServiceUnitTest, UnspecifiedToolRegistrationChecks) {
  CustomTestTool unspecified_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED);
  EXPECT_CHECK_DEATH(service().RegisterTool(&unspecified_tool));
}

// Tests that server suggestions are not requested when the feature is disabled.
TEST_F(SuggestionServiceUnitTest, ServerSuggestionsDisabledByDefault) {
  CustomTestTool tool;
  service().RegisterTool(&tool);

  EXPECT_CALL(mock_model_executor(), ExecuteModel).Times(0);

  AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(), IsEmpty());

  service().UnregisterTool(&tool);
}

// Tests that server suggestions are not requested when the feature is
// group-controlled and the profile is not in the group.
TEST_F(SuggestionServiceUnitTest,
       ServerSuggestionsNotRequestedWhenNotInGoogleGroup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kSmartSelectionServerSuggestions,
      {{variations::internal::kGoogleGroupFeatureParamName, "1234"}});

  CustomTestTool tool;
  service().RegisterTool(&tool);

  EXPECT_CALL(mock_model_executor(), ExecuteModel).Times(0);

  AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(), IsEmpty());

  service().UnregisterTool(&tool);
}

// Tests that server suggestions fall back to `base::FeatureList::IsEnabled`
// when `GoogleGroupsManager` is null.
TEST_F(SuggestionServiceUnitTest,
       ServerSuggestionsFallbackWhenGoogleGroupsManagerIsNull) {
  base::test::ScopedFeatureList feature_list{kSmartSelectionServerSuggestions};

  tabs::MockTabInterface tab_without_groups;
  SuggestionService service_without_groups(&tab_without_groups,
                                           &mock_model_executor(),
                                           /*google_groups_manager=*/nullptr);
  CustomTestTool gemini_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service_without_groups.RegisterTool(&gemini_tool);

  optimization_guide::proto::SmartSelectionSuggestionsResponse response;
  optimization_guide::proto::SmartSelectionSuggestion* s1 =
      response.add_suggestions();
  s1->set_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  s1->set_label("Server Gemini Action");

  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                               kSmartSelectionSuggestions,
                           _, _, _))
      .WillOnce(RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              base::ok(optimization_guide::AnyWrapProto(response)),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service_without_groups.RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(),
              ElementsAre(SuggestionWithLabel(u"Server Gemini Action")));

  service_without_groups.UnregisterTool(&gemini_tool);
}

// Tests that server suggestions are not requested when no tools are registered.
TEST_F(SuggestionServiceUnitTest,
       ServerSuggestionsNotRequestedWhenNoToolsRegistered) {
  base::test::ScopedFeatureList feature_list{kSmartSelectionServerSuggestions};

  EXPECT_CALL(mock_model_executor(), ExecuteModel).Times(0);

  AreaOfInterest aoi;
  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(), IsEmpty());
}

// Tests that server suggestions are requested, parsed, and returned.
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsWithServerSuggestions) {
  base::test::ScopedFeatureList feature_list{kSmartSelectionServerSuggestions};

  CustomTestTool gemini_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
      /*capabilities=*/{"youtube_tool", "workspace_tool"});
  service().RegisterTool(&gemini_tool);

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
  s3->set_tool(optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED);
  s3->set_label("Unspecified Tool Action");

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
                        ElementsAre(ToolWithCapabilities(
                            optimization_guide::proto::
                                SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
                            ElementsAre("youtube_tool", "workspace_tool"))));
            ASSERT_EQ(request.areas_of_interest().size(), 1);
            const optimization_guide::proto::AreaOfInterest& proto_aoi =
                request.areas_of_interest(0);
            EXPECT_FALSE(proto_aoi.image_bytes().empty());
            EXPECT_EQ(proto_aoi.mime_type(), "image/png");
            SkBitmap decoded = gfx::PNGCodec::Decode(
                base::as_byte_span(proto_aoi.image_bytes()));
            EXPECT_EQ(decoded.width(), 50);
            EXPECT_EQ(decoded.height(), 50);
            EXPECT_EQ(proto_aoi.selection().x(), 10);
            EXPECT_EQ(proto_aoi.selection().y(), 20);
            EXPECT_EQ(proto_aoi.selection().width(), 30);
            EXPECT_EQ(proto_aoi.selection().height(), 40);
            EXPECT_TRUE(proto_aoi.has_annotated_page_content());
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

  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(),
              ElementsAre(SuggestionWithLabel(u"Server Gemini Action")));

  service().UnregisterTool(&gemini_tool);
}

// Tests that screenshots are encoded as JPEG when
// `kSmartSelectionServerJpegQuality` is in [0, 100].
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsWithJpegScreenshot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kSmartSelectionServerSuggestions,
      {{kSmartSelectionServerJpegQuality.name, "85"}});

  CustomTestTool gemini_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service().RegisterTool(&gemini_tool);

  optimization_guide::proto::SmartSelectionSuggestionsResponse response;
  optimization_guide::proto::SmartSelectionSuggestion* s1 =
      response.add_suggestions();
  s1->set_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  s1->set_label("Jpeg Action");

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
            const auto& request =
                static_cast<const optimization_guide::proto::
                                SmartSelectionSuggestionsRequest&>(
                    request_metadata);
            ASSERT_EQ(request.areas_of_interest().size(), 1);
            const optimization_guide::proto::AreaOfInterest& proto_aoi =
                request.areas_of_interest(0);
            EXPECT_FALSE(proto_aoi.image_bytes().empty());
            EXPECT_EQ(proto_aoi.mime_type(), "image/jpeg");
            SkBitmap decoded = gfx::JPEGCodec::Decode(
                base::as_byte_span(proto_aoi.image_bytes()));
            EXPECT_FALSE(decoded.isNull());
            EXPECT_EQ(decoded.width(), 50);
            EXPECT_EQ(decoded.height(), 50);
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

  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());
  EXPECT_THAT(future.Take(), ElementsAre(SuggestionWithLabel(u"Jpeg Action")));

  service().UnregisterTool(&gemini_tool);
}

// Tests that `annotated_page_content` is omitted when
// `kSmartSelectionServerAppendApc` is false.
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsWithoutApc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kSmartSelectionServerSuggestions,
      {{kSmartSelectionServerAppendApc.name, "false"}});

  CustomTestTool gemini_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service().RegisterTool(&gemini_tool);

  optimization_guide::proto::SmartSelectionSuggestionsResponse response;
  optimization_guide::proto::SmartSelectionSuggestion* s1 =
      response.add_suggestions();
  s1->set_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  s1->set_label("Action Without APC");

  EXPECT_CALL(mock_model_executor(),
              ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                               kSmartSelectionSuggestions,
                           _, _, _))
      .WillOnce([&](optimization_guide::ModelBasedCapabilityKey feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const optimization_guide::ModelExecutionOptions& options,
                    optimization_guide::
                        OptimizationGuideModelExecutionResultCallback
                            callback) {
        const auto& request = static_cast<
            const optimization_guide::proto::SmartSelectionSuggestionsRequest&>(
            request_metadata);
        ASSERT_EQ(request.areas_of_interest().size(), 1);
        EXPECT_FALSE(request.areas_of_interest(0).has_annotated_page_content());
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

  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(),
              ElementsAre(SuggestionWithLabel(u"Action Without APC")));

  service().UnregisterTool(&gemini_tool);
}

// Tests that screenshot cropping and selection coordinate rebasing work when
// `kSmartSelectionServerCropPadding` is non-negative.
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsWithCroppedScreenshot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kSmartSelectionServerSuggestions,
      {{kSmartSelectionServerCropPadding.name, "15"}});

  CustomTestTool gemini_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service().RegisterTool(&gemini_tool);

  optimization_guide::proto::SmartSelectionSuggestionsResponse response;
  optimization_guide::proto::SmartSelectionSuggestion* s1 =
      response.add_suggestions();
  s1->set_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  s1->set_label("Cropped Action");

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
            const auto& request =
                static_cast<const optimization_guide::proto::
                                SmartSelectionSuggestionsRequest&>(
                    request_metadata);
            ASSERT_EQ(request.areas_of_interest().size(), 1);
            const optimization_guide::proto::AreaOfInterest& proto_aoi =
                request.areas_of_interest(0);
            EXPECT_EQ(proto_aoi.mime_type(), "image/png");
            SkBitmap decoded = gfx::PNGCodec::Decode(
                base::as_byte_span(proto_aoi.image_bytes()));
            // Selection (10, 20, 30, 40) outset by 15 is (-5, 5, 60, 70),
            // which clamps to (0, 5, 55, 70) within the 100x100 screenshot.
            EXPECT_EQ(decoded.width(), 55);
            EXPECT_EQ(decoded.height(), 70);
            EXPECT_EQ(proto_aoi.selection().x(), 10);
            EXPECT_EQ(proto_aoi.selection().y(), 15);
            EXPECT_EQ(proto_aoi.selection().width(), 30);
            EXPECT_EQ(proto_aoi.selection().height(), 40);
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::ok(optimization_guide::AnyWrapProto(response)),
                    /*execution_info=*/nullptr),
                /*log_entry=*/nullptr);
          });

  AreaOfInterest aoi;
  aoi.screenshot.allocN32Pixels(100, 100);
  aoi.screenshot.eraseColor(SK_ColorRED);
  aoi.bounds = gfx::Rect(10, 20, 30, 40);

  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(),
              ElementsAre(SuggestionWithLabel(u"Cropped Action")));

  service().UnregisterTool(&gemini_tool);
}

// Tests that screenshot downscaling and selection scaling work when
// `kSmartSelectionServerMaxScreenshotDimension` is non-negative and exceeded.
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsWithDownscaledScreenshot) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kSmartSelectionServerSuggestions,
      {{kSmartSelectionServerMaxScreenshotDimension.name, "100"}});

  CustomTestTool gemini_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service().RegisterTool(&gemini_tool);

  optimization_guide::proto::SmartSelectionSuggestionsResponse response;
  optimization_guide::proto::SmartSelectionSuggestion* s1 =
      response.add_suggestions();
  s1->set_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  s1->set_label("Downscaled Action");

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
            const auto& request =
                static_cast<const optimization_guide::proto::
                                SmartSelectionSuggestionsRequest&>(
                    request_metadata);
            ASSERT_EQ(request.areas_of_interest().size(), 1);
            const optimization_guide::proto::AreaOfInterest& proto_aoi =
                request.areas_of_interest(0);
            SkBitmap decoded = gfx::PNGCodec::Decode(
                base::as_byte_span(proto_aoi.image_bytes()));
            // Original 200x100 downscaled by 0.5 to fit within 100.
            EXPECT_EQ(decoded.width(), 100);
            EXPECT_EQ(decoded.height(), 50);
            EXPECT_EQ(proto_aoi.selection().x(), 10);
            EXPECT_EQ(proto_aoi.selection().y(), 10);
            EXPECT_EQ(proto_aoi.selection().width(), 30);
            EXPECT_EQ(proto_aoi.selection().height(), 20);
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::ok(optimization_guide::AnyWrapProto(response)),
                    /*execution_info=*/nullptr),
                /*log_entry=*/nullptr);
          });

  AreaOfInterest aoi;
  aoi.screenshot.allocN32Pixels(200, 100);
  aoi.screenshot.eraseColor(SK_ColorRED);
  aoi.bounds = gfx::Rect(20, 20, 60, 40);

  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(),
              ElementsAre(SuggestionWithLabel(u"Downscaled Action")));

  service().UnregisterTool(&gemini_tool);
}

// Tests that screenshot is not downscaled when its longest side does not exceed
// `kSmartSelectionServerMaxScreenshotDimension`.
TEST_F(SuggestionServiceUnitTest,
       RequestSuggestionsScreenshotBelowMaxDimensionNotDownscaled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kSmartSelectionServerSuggestions,
      {{kSmartSelectionServerMaxScreenshotDimension.name, "200"}});

  CustomTestTool gemini_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service().RegisterTool(&gemini_tool);

  optimization_guide::proto::SmartSelectionSuggestionsResponse response;
  optimization_guide::proto::SmartSelectionSuggestion* s1 =
      response.add_suggestions();
  s1->set_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  s1->set_label("Not Downscaled Action");

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
            const auto& request =
                static_cast<const optimization_guide::proto::
                                SmartSelectionSuggestionsRequest&>(
                    request_metadata);
            ASSERT_EQ(request.areas_of_interest().size(), 1);
            const optimization_guide::proto::AreaOfInterest& proto_aoi =
                request.areas_of_interest(0);
            SkBitmap decoded = gfx::PNGCodec::Decode(
                base::as_byte_span(proto_aoi.image_bytes()));
            EXPECT_EQ(decoded.width(), 100);
            EXPECT_EQ(decoded.height(), 80);
            EXPECT_EQ(proto_aoi.selection().x(), 10);
            EXPECT_EQ(proto_aoi.selection().y(), 10);
            EXPECT_EQ(proto_aoi.selection().width(), 20);
            EXPECT_EQ(proto_aoi.selection().height(), 20);
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::ok(optimization_guide::AnyWrapProto(response)),
                    /*execution_info=*/nullptr),
                /*log_entry=*/nullptr);
          });

  AreaOfInterest aoi;
  aoi.screenshot.allocN32Pixels(100, 80);
  aoi.screenshot.eraseColor(SK_ColorRED);
  aoi.bounds = gfx::Rect(10, 10, 20, 20);

  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(),
              ElementsAre(SuggestionWithLabel(u"Not Downscaled Action")));

  service().UnregisterTool(&gemini_tool);
}

// Tests that screenshot cropping and downscaling work together properly.
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsWithCropAndDownscale) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kSmartSelectionServerSuggestions,
      {{kSmartSelectionServerCropPadding.name, "10"},
       {kSmartSelectionServerMaxScreenshotDimension.name, "50"}});

  CustomTestTool gemini_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  service().RegisterTool(&gemini_tool);

  optimization_guide::proto::SmartSelectionSuggestionsResponse response;
  optimization_guide::proto::SmartSelectionSuggestion* s1 =
      response.add_suggestions();
  s1->set_tool(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  s1->set_label("Cropped and Downscaled Action");

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
            const auto& request =
                static_cast<const optimization_guide::proto::
                                SmartSelectionSuggestionsRequest&>(
                    request_metadata);
            ASSERT_EQ(request.areas_of_interest().size(), 1);
            const optimization_guide::proto::AreaOfInterest& proto_aoi =
                request.areas_of_interest(0);
            SkBitmap decoded = gfx::PNGCodec::Decode(
                base::as_byte_span(proto_aoi.image_bytes()));
            // Original: 200x200, selection (40, 40, 60, 60).
            // Padded selection with padding 10: (30, 30, 80, 80).
            // Cropped size: 80x80. Selection relative to crop: (10, 10, 60,
            // 60). Downscaled to max dimension 50: scale 50 / 80 = 0.625.
            // Downscaled size: 50x50.
            // Scaled selection: (round(10 * 0.625), round(10 * 0.625),
            //                    round(70 * 0.625) - 6, round(70 * 0.625) - 6)
            //                 = (6, 6, 38, 38).
            EXPECT_EQ(decoded.width(), 50);
            EXPECT_EQ(decoded.height(), 50);
            EXPECT_EQ(proto_aoi.selection().x(), 6);
            EXPECT_EQ(proto_aoi.selection().y(), 6);
            EXPECT_EQ(proto_aoi.selection().width(), 38);
            EXPECT_EQ(proto_aoi.selection().height(), 38);
            std::move(callback).Run(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::ok(optimization_guide::AnyWrapProto(response)),
                    /*execution_info=*/nullptr),
                /*log_entry=*/nullptr);
          });

  AreaOfInterest aoi;
  aoi.screenshot.allocN32Pixels(200, 200);
  aoi.screenshot.eraseColor(SK_ColorRED);
  aoi.bounds = gfx::Rect(40, 40, 60, 60);

  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(
      future.Take(),
      ElementsAre(SuggestionWithLabel(u"Cropped and Downscaled Action")));

  service().UnregisterTool(&gemini_tool);
}

// Tests that server errors still complete the suggestion request.
TEST_F(SuggestionServiceUnitTest, RequestSuggestionsServerError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kSmartSelectionServerSuggestions,
      {{kSmartSelectionServerTimeout.name, "5s"}});

  CustomTestTool gemini_tool(
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
      .WillOnce(RunOnceCallback<3>(
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
  TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
  service().RequestSuggestions(aoi, future.GetCallback());

  EXPECT_THAT(future.Take(), IsEmpty());

  service().UnregisterTool(&gemini_tool);
}

class SuggestionServiceMetricsTest : public ChromeRenderViewHostTestHarness {
 public:
  SuggestionServiceMetricsTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    target_prefs_.registry()->RegisterDictionaryPref(
        variations::prefs::kVariationsGoogleGroups);
    GoogleGroupsManager::RegisterProfilePrefs(source_prefs_.registry());
    google_groups_manager_.emplace(target_prefs_, "Default", source_prefs_);
    service_.emplace(&mock_tab_, &mock_model_executor_,
                     &google_groups_manager_.value());
    service_->RegisterTool(&gemini_tool_);
  }

  void TearDown() override {
    service_->UnregisterTool(&gemini_tool_);
    service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  optimization_guide::proto::SmartSelectionSuggestionsResponse
  RequestSuggestion(
      const optimization_guide::proto::SmartSelectionLatencyMetrics&
          latency_metrics) {
    optimization_guide::proto::SmartSelectionSuggestionsResponse response;
    auto* s1 = response.add_suggestions();
    s1->set_tool(
        optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
    s1->set_label("Action");

    *response.mutable_latency_metrics() = latency_metrics;

    EXPECT_CALL(mock_model_executor_,
                ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                                 kSmartSelectionSuggestions,
                             _, _, _))
        .WillOnce(DoAll(
            [this](auto&&...) {
              task_environment()->AdvanceClock(base::Milliseconds(950));
            },
            RunOnceCallback<3>(
                optimization_guide::OptimizationGuideModelExecutionResult(
                    base::ok(optimization_guide::AnyWrapProto(response)),
                    /*execution_info=*/nullptr),
                /*log_entry=*/nullptr)));

    AreaOfInterest aoi;
    aoi.screenshot.allocN32Pixels(100, 100);
    aoi.bounds = gfx::Rect(10, 10, 20, 20);

    TestFuture<std::vector<std::unique_ptr<Suggestion>>> future;
    service_->RequestSuggestions(aoi, future.GetCallback());
    task_environment()->AdvanceClock(base::Milliseconds(300));
    EXPECT_THAT(future.Take(), ElementsAre(SuggestionWithLabel(u"Action")));
    return response;
  }

  void EmitMetrics() {
    NavigateAndCommit(GURL("https://example.com/test"));

    optimization_guide::proto::SmartSelectionLatencyMetrics latency_metrics;
    latency_metrics.set_mes_total_ms(800);
    latency_metrics.set_model_execution_ms(500);
    latency_metrics.set_mes_image_preprocessing_ms(50);
    latency_metrics.set_model_content_processing_ms(100);
    latency_metrics.set_model_prefill_queue_wait_ms(30);
    latency_metrics.set_model_prefill_ms(120);
    latency_metrics.set_model_decode_queue_wait_ms(20);
    latency_metrics.set_model_decode_ms(200);
    RequestSuggestion(latency_metrics);
  }

  tabs::MockTabInterface& mock_tab() { return mock_tab_; }

 private:
  base::test::ScopedFeatureList feature_list_{kSmartSelectionServerSuggestions};
  TestingPrefServiceSimple target_prefs_;
  sync_preferences::TestingPrefServiceSyncable source_prefs_;
  std::optional<GoogleGroupsManager> google_groups_manager_;
  tabs::MockTabInterface mock_tab_;
  optimization_guide::MockRemoteModelExecutor mock_model_executor_;
  std::optional<SuggestionService> service_;
  CustomTestTool gemini_tool_{
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME};
};

// Tests that when WebContents is null (e.g. detached tab), UMA is recorded but
// no UKM is emitted and it does not crash.
TEST_F(SuggestionServiceMetricsTest,
       RequestSuggestionsWithoutWebContentsDoesNotRecordUkm) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  RequestSuggestion({});

  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.EndToEnd", 1);

  EXPECT_THAT(ukm_recorder.GetEntriesByName(
                  ukm::builders::SmartSelection_Suggestions::kEntryName),
              IsEmpty());
}

// Tests that successful suggestion requests record UMA metrics with all
// breakdown stages.
TEST_F(SuggestionServiceMetricsTest, RecordsUmaMetricsOnSuccess) {
  ON_CALL(mock_tab(), GetContents()).WillByDefault(Return(web_contents()));
  base::HistogramTester histogram_tester;
  EmitMetrics();
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.EndToEnd", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.ClientPrep", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.ClientMesRpc", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.MesTotal", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.Transmission", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.ModelRun", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.MesOverhead", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.MesImagePrep", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.MesOtherOverhead", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.ModelContentProc", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.ModelPrefillQueueWait", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.ModelPrefill", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.ModelDecodeQueueWait", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.ModelDecode", 1);
  histogram_tester.ExpectTotalCount(
      "SmartSelection.Suggestions.Latency.ModelOther", 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.Response.SuggestionCount", 1, 1);
}

// Tests that successful suggestion requests record UKM metrics with all
// breakdown stages.
TEST_F(SuggestionServiceMetricsTest, RecordsUkmMetricsOnSuccess) {
  ON_CALL(mock_tab(), GetContents()).WillByDefault(Return(web_contents()));
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EmitMetrics();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SmartSelection_Suggestions::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0]->source_id,
            web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_EndToEndMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(300 + 950));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_ClientPrepMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(300));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_ClientMesRpcMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(950));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_MesTotalMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(800));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_TransmissionMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(950 - 800));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_ModelRunMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(500));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_MesOverheadMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(800 - 500));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_MesImagePrepMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(50));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::
          kLatency_MesOtherOverheadMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(300 - 50));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::
          kLatency_ModelContentProcMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(100));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::
          kLatency_ModelPrefillQueueWaitMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(30));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_ModelPrefillMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(120));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::
          kLatency_ModelDecodeQueueWaitMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(20));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_ModelDecodeMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(200));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kLatency_ModelOtherMsName,
      ukm::GetExponentialBucketMinForFineUserTiming(500 - 100 - 30 - 120 - 20 -
                                                    200));
  ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::SmartSelection_Suggestions::kResponse_SuggestionCountName,
      1);
}

}  // namespace
}  // namespace selection
