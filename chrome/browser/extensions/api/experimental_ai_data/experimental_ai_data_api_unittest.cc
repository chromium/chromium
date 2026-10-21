// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experimental_ai_data/experimental_ai_data_api.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/version_info/channel.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/page_content_annotations/content/page_context_fetcher_metrics.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace extensions {
namespace {

constexpr char kApcExtensionId[] = "bbnmkciocedkjdlapchelmdflhahacpa";
constexpr char kPageChangedError[] = "Page changed during data collection.";
constexpr char kPolicyError[] = "Host access is restricted by policy.";

// Fake only tab lookup, screenshot access, and the fetch operation. The API's
// validation, option construction, serialization, and response checks run as in
// production, including the inherited experimentalAiData allowlist checks.
class TestGetApcSnapshotFunction
    : public ExperimentalAiDataGetApcSnapshotFunction {
 public:
  void set_web_contents(content::WebContents* contents) {
    contents_ = contents;
  }

  bool snapshot_started = false;
  int resolved_tab_id = -1;
  blink::mojom::AIPageContentOptionsPtr apc_options;
  std::optional<page_content_annotations::ScreenshotOptions> screenshot_options;
  std::optional<ScreenshotAccessError> screenshot_access_error;
  std::optional<optimization_guide::AIPageContentError> apc_error;
  std::optional<page_content_annotations::ScreenshotError> screenshot_error;
  std::string context_error;
  base::OnceClosure before_fetch;
  base::OnceClosure after_fetch;

 protected:
  ~TestGetApcSnapshotFunction() override = default;

  content::WebContents* GetTabById(int tab_id) override {
    resolved_tab_id = tab_id;
    return contents_;
  }

  base::expected<void, ScreenshotAccessError> CheckScreenshotAccess(
      content::WebContents* contents) const override {
    EXPECT_EQ(contents_, contents);
    if (screenshot_access_error) {
      return base::unexpected(*screenshot_access_error);
    }
    return base::ok();
  }

  void FetchSnapshot(
      content::WebContents* contents,
      const page_content_annotations::FetchPageContextOptions& options,
      page_content_annotations::FetchPageContextResultCallback callback)
      override {
    EXPECT_EQ(contents_, contents);
    snapshot_started = true;
    apc_options = options.annotated_page_content_options.Clone();
    screenshot_options = options.screenshot_options;
    if (!context_error.empty()) {
      std::move(callback).Run(base::unexpected(
          page_content_annotations::FetchPageContextErrorDetails{
              page_content_annotations::FetchPageContextError::
                  kWebContentsChanged,
              context_error}));
      EXPECT_FALSE(did_respond());
      return;
    }

    auto snapshot =
        std::make_unique<page_content_annotations::FetchPageContextResult>();
    if (!apc_error) {
      optimization_guide::AIPageContentResult apc;
      apc.proto.mutable_root_node();
      snapshot->annotated_page_content_result.emplace(std::move(apc));
    } else {
      snapshot->annotated_page_content_result = base::unexpected(*apc_error);
    }
    if (screenshot_options) {
      if (!screenshot_error) {
        snapshot->screenshot_result.emplace(gfx::Size(1, 1));
        snapshot->screenshot_result->screenshot_data = {'p', 'n', 'g'};
      } else {
        snapshot->screenshot_result = base::unexpected(*screenshot_error);
      }
    }
    if (before_fetch) {
      std::move(before_fetch).Run();
    }
    std::move(callback).Run(base::ok(std::move(snapshot)));
    EXPECT_FALSE(did_respond());
    // Queue behind fetch completion but ahead of its worker's UI reply, so the
    // page or policy changes while serialization is outstanding.
    if (after_fetch) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(after_fetch));
    }
  }

 private:
  raw_ptr<content::WebContents> contents_ = nullptr;
};

class ExperimentalAiDataApcTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Individual tests decide whether policy allows or blocks this site.
    NavigateAndCommit(GURL("https://example.test/"));
    extension_ = ExtensionBuilder("APC Debugging Extension")
                     .SetID(kApcExtensionId)
                     .AddAPIPermission("experimentalAiData")
                     .Build();
  }

  scoped_refptr<TestGetApcSnapshotFunction> CreateFunction() {
    auto function = base::MakeRefCounted<TestGetApcSnapshotFunction>();
    function->set_extension(extension_);
    function->set_web_contents(web_contents());
    return function;
  }

  void BlockHost(const char* pattern = "*://example.test/*") {
    extension_->permissions_data()->SetPolicyHostRestrictions(
        URLPatternSet({URLPattern(URLPattern::SCHEME_ALL, pattern)}), {});
  }

  std::optional<base::Value> Run(TestGetApcSnapshotFunction* function,
                                 const std::string& args = "[7, {}]") {
    return api_test_utils::RunFunctionAndReturnSingleResult(function, args,
                                                            profile());
  }

  std::string RunError(TestGetApcSnapshotFunction* function,
                       const std::string& args = "[7, {}]") {
    return api_test_utils::RunFunctionAndReturnError(function, args, profile());
  }

  ScopedCurrentChannel channel_{version_info::Channel::DEV};
  scoped_refptr<const Extension> extension_;
};

TEST_F(ExperimentalAiDataApcTest,
       DebuggingExtensionGetsActionableApcByDefault) {
  for (const char* args :
       {"[7, {}]", R"([7, {"excludeActionableDetails": false}])",
        R"([7, {"excludeAdRelated": false}])"}) {
    SCOPED_TRACE(args);
    auto function = CreateFunction();
    auto result = Run(function.get(), args);
    ASSERT_TRUE(result) << function->GetError();
    ASSERT_TRUE(result->is_dict());
    const std::string* apc = result->GetDict().FindString("apcBase64");
    ASSERT_TRUE(apc);
    std::string bytes;
    ASSERT_TRUE(base::Base64Decode(*apc, &bytes));
    optimization_guide::proto::AnnotatedPageContent proto;
    ASSERT_TRUE(proto.ParseFromString(bytes));
    EXPECT_TRUE(proto.has_root_node());
    EXPECT_FALSE(result->GetDict().contains("screenshotBase64"));
    EXPECT_EQ(7, function->resolved_tab_id);
    ASSERT_TRUE(function->apc_options);
    EXPECT_EQ(blink::mojom::AIPageContentMode::kActionableElements,
              function->apc_options->mode);
    EXPECT_EQ(0, function->apc_options->max_meta_elements);
    EXPECT_FALSE(function->apc_options->include_same_site_only);
    EXPECT_FALSE(function->apc_options->node_id_allowlist);
    EXPECT_FALSE(function->apc_options->non_salient_content_config);
    EXPECT_FALSE(function->screenshot_options);
  }
}

TEST_F(ExperimentalAiDataApcTest,
       ScreenshotModesExtractApcAndRequestLosslessPng) {
  struct TestCase {
    const char* args;
    bool expect_apc;
  };
  constexpr TestCase kTestCases[] = {
      {.args = R"([7, {"type": "screenshot"}])", .expect_apc = false},
      {.args = R"([7, {"type": "apc_and_screenshot"}])", .expect_apc = true},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.args);
    auto function = CreateFunction();
    auto result = Run(function.get(), test_case.args);
    ASSERT_TRUE(result) << function->GetError();
    ASSERT_TRUE(result->is_dict());
    EXPECT_EQ(test_case.expect_apc, result->GetDict().contains("apcBase64"));
    const std::string* screenshot_base64 =
        result->GetDict().FindString("screenshotBase64");
    ASSERT_TRUE(screenshot_base64);
    EXPECT_EQ("cG5n", *screenshot_base64);
    ASSERT_TRUE(function->apc_options);
    ASSERT_TRUE(function->screenshot_options);
    EXPECT_FALSE(function->screenshot_options->capture_full_page());
    const auto& screenshot =
        function->screenshot_options->screenshot_collection_options();
    ASSERT_TRUE(screenshot);
    using ScreenshotOptions = page_content_annotations::ScreenshotOptions;
    EXPECT_EQ(ScreenshotOptions::ScreenshotImageFormat::kPng,
              screenshot->screenshot_image_format);
    EXPECT_EQ(ScreenshotOptions::ScreenshotCompressionQuality::kNone,
              screenshot->screenshot_compression_quality);
  }
}

TEST_F(ExperimentalAiDataApcTest, MapsExtractionOptions) {
  auto function = CreateFunction();
  auto result = Run(function.get(), R"([7, {
    "type": "apc_and_screenshot",
    "excludeActionableDetails": true,
    "excludeCrossSiteFrames": true,
    "excludeAdRelated": true,
    "maxMetaElements": 12
  }])");
  ASSERT_TRUE(result) << function->GetError();
  const auto& options = function->apc_options;
  ASSERT_TRUE(options);
  EXPECT_EQ(blink::mojom::AIPageContentMode::kDefault, options->mode);
  EXPECT_TRUE(options->include_same_site_only);
  ASSERT_TRUE(options->non_salient_content_config);
  EXPECT_TRUE(options->non_salient_content_config->exclude_ad_related);
  EXPECT_EQ(12, options->max_meta_elements);
  EXPECT_FALSE(options->node_id_allowlist);
}

TEST_F(ExperimentalAiDataApcTest, RejectsIncognitoCaller) {
  auto function = CreateFunction();
  // An incognito caller cannot capture even a regular-profile tab.
  EXPECT_EQ("Incognito profile not supported.",
            api_test_utils::RunFunctionAndReturnError(
                function.get(), "[7, {}]",
                profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
                api_test_utils::FunctionMode::kIncognito));
  EXPECT_FALSE(function->snapshot_started);
}

TEST_F(ExperimentalAiDataApcTest, RejectsIncognitoTargetInAllModes) {
  auto incognito_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  for (const char* args : {"[7, {}]", R"([7, {"type": "screenshot"}])",
                           R"([7, {"type": "apc_and_screenshot"}])"}) {
    SCOPED_TRACE(args);
    auto function = CreateFunction();
    // The fake lookup deliberately returns an incognito tab to exercise the
    // capture boundary independently of tab enumeration.
    function->set_web_contents(incognito_contents.get());
    EXPECT_EQ("Incognito profile not supported.",
              RunError(function.get(), args));
    EXPECT_FALSE(function->snapshot_started);
  }
}

TEST_F(ExperimentalAiDataApcTest, MatchingBlockedHostRejectsAllModes) {
  BlockHost();
  for (const char* args : {"[7, {}]", R"([7, {"type": "screenshot"}])",
                           R"([7, {"type": "apc_and_screenshot"}])",
                           R"([7, {"excludeCrossSiteFrames": true}])"}) {
    SCOPED_TRACE(args);
    auto function = CreateFunction();
    EXPECT_EQ(kPolicyError, RunError(function.get(), args));
    EXPECT_FALSE(function->snapshot_started);
  }
}

TEST_F(ExperimentalAiDataApcTest, RejectsBlockedNestedFrameInAllModes) {
  auto* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  auto* nested =
      content::RenderFrameHostTester::For(child)->AppendChild("nested");
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://blocked.test/"), nested);
  BlockHost("*://blocked.test/*");

  for (const char* args : {"[7, {}]", R"([7, {"type": "screenshot"}])",
                           R"([7, {"type": "apc_and_screenshot"}])",
                           R"([7, {"excludeCrossSiteFrames": true}])"}) {
    SCOPED_TRACE(args);
    auto function = CreateFunction();
    EXPECT_EQ(kPolicyError, RunError(function.get(), args));
    EXPECT_FALSE(function->snapshot_started);
  }
}

TEST_F(ExperimentalAiDataApcTest,
       RejectsPolicyChangesDuringCaptureAndSerialization) {
  for (bool during_capture : {true, false}) {
    SCOPED_TRACE(during_capture ? "During capture" : "During serialization");
    for (const char* args : {"[7, {}]", R"([7, {"type": "screenshot"}])",
                             R"([7, {"type": "apc_and_screenshot"}])"}) {
      SCOPED_TRACE(args);
      extension_->permissions_data()->SetPolicyHostRestrictions({}, {});
      auto function = CreateFunction();
      auto& closure =
          during_capture ? function->before_fetch : function->after_fetch;
      closure = base::BindLambdaForTesting([&] { BlockHost(); });
      EXPECT_EQ(kPolicyError, RunError(function.get(), args));
      EXPECT_TRUE(function->snapshot_started);
    }
  }
}

TEST_F(ExperimentalAiDataApcTest,
       AllowsUnrelatedPolicyChangesDuringCaptureAndSerialization) {
  for (bool during_capture : {true, false}) {
    SCOPED_TRACE(during_capture ? "During capture" : "During serialization");
    extension_->permissions_data()->SetPolicyHostRestrictions({}, {});
    auto function = CreateFunction();
    auto& closure =
        during_capture ? function->before_fetch : function->after_fetch;
    closure =
        base::BindLambdaForTesting([&] { BlockHost("*://unrelated.test/*"); });
    EXPECT_TRUE(Run(function.get())) << function->GetError();
  }
}

TEST_F(ExperimentalAiDataApcTest,
       RejectsBlockedChildNavigationDuringCaptureAndSerialization) {
  BlockHost("*://blocked.test/*");
  for (bool during_capture : {true, false}) {
    SCOPED_TRACE(during_capture ? "During capture" : "During serialization");
    NavigateAndCommit(GURL("https://example.test/"));
    auto* child =
        content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
    auto function = CreateFunction();
    auto& closure =
        during_capture ? function->before_fetch : function->after_fetch;
    closure = base::BindLambdaForTesting([&] {
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://blocked.test/"), child);
    });
    EXPECT_EQ(kPolicyError, RunError(function.get()));
    EXPECT_TRUE(function->snapshot_started);
  }
}

TEST_F(ExperimentalAiDataApcTest, RejectsPageReplacementDuringSerialization) {
  auto function = CreateFunction();
  function->after_fetch = base::BindLambdaForTesting(
      [&] { NavigateAndCommit(GURL("https://other.test/")); });
  EXPECT_EQ(kPageChangedError, RunError(function.get()));
}

TEST_F(ExperimentalAiDataApcTest, RejectsTabClosureDuringSerialization) {
  auto function = CreateFunction();
  function->after_fetch = base::BindLambdaForTesting([&] {
    function->set_web_contents(nullptr);
    DeleteContents();
  });
  EXPECT_EQ(kPageChangedError, RunError(function.get()));
}

TEST_F(ExperimentalAiDataApcTest, ReportsCaptureFailure) {
  auto function = CreateFunction();
  function->context_error = "web contents changed";
  EXPECT_EQ("Failed to capture page context: web contents changed",
            RunError(function.get()));
}

TEST_F(ExperimentalAiDataApcTest,
       ApcFailureRejectsAllModesIncludingScreenshotOnly) {
  for (const char* args : {"[7, {}]", R"([7, {"type": "screenshot"}])",
                           R"([7, {"type": "apc_and_screenshot"}])"}) {
    SCOPED_TRACE(args);
    auto function = CreateFunction();
    function->apc_error = optimization_guide::AIPageContentError{
        optimization_guide::AIPageContentExtractionStatus::
            kMainFrameEmptyOrDisconnectedRendererResponse,
        "renderer disconnected"};
    EXPECT_EQ("Failed to extract APC: renderer disconnected",
              RunError(function.get(), args));
  }
}

TEST_F(ExperimentalAiDataApcTest, ReportsScreenshotFailure) {
  auto function = CreateFunction();
  function->screenshot_error = page_content_annotations::ScreenshotError{
      page_content_annotations::ScreenshotCaptureStatus::kTimeout,
      "ScreenshotTimeout"};
  EXPECT_EQ("Failed to capture tab: ScreenshotTimeout",
            RunError(function.get(), R"([7, {"type": "screenshot"}])"));
}

TEST_F(ExperimentalAiDataApcTest,
       ScreenshotRestrictionsApplyBeforeAndAfterCapture) {
  struct TestCase {
    ScreenshotAccessError restriction;
    const char* expected_error;
  };
  constexpr TestCase kTestCases[] = {
      {.restriction = ScreenshotAccessError::kDisabledByPreferences,
       .expected_error = "Failed to capture tab: screenshots disabled"},
      {.restriction = ScreenshotAccessError::kDisabledByDlp,
       .expected_error = "Failed to capture tab: screenshots disabled by DLP"},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.expected_error);
    for (bool before_capture : {true, false}) {
      SCOPED_TRACE(before_capture ? "Before capture" : "After capture");
      auto function = CreateFunction();
      if (before_capture) {
        function->screenshot_access_error = test_case.restriction;
      } else {
        function->after_fetch = base::BindLambdaForTesting(
            [&] { function->screenshot_access_error = test_case.restriction; });
      }
      EXPECT_EQ(test_case.expected_error,
                RunError(function.get(), R"([7, {"type": "screenshot"}])"));
      EXPECT_EQ(!before_capture, function->snapshot_started);
    }
  }
}

TEST_F(ExperimentalAiDataApcTest, ScreenshotRestrictionDoesNotBlockApcOnly) {
  auto function = CreateFunction();
  function->screenshot_access_error = ScreenshotAccessError::kDisabledByDlp;
  EXPECT_TRUE(Run(function.get())) << function->GetError();
}

class FakeSurroundingTextLocalFrame : public content::FakeLocalFrame {
 public:
  explicit FakeSurroundingTextLocalFrame(content::RenderFrameHost* rfh) {
    Init(rfh->GetRemoteAssociatedInterfaces());
  }

  void GetTextSurroundingSelection(
      uint32_t max_length,
      GetTextSurroundingSelectionCallback callback) override {
    received_max_length = max_length;
    const bool should_drop = drop_callback;
    if (on_request) {
      std::move(on_request).Run();
    }
    if (should_drop) {
      return;
    }
    std::move(callback).Run(surrounding_text, start_offset, end_offset);
  }

  std::u16string surrounding_text;
  uint32_t start_offset = 0;
  uint32_t end_offset = 0;
  std::optional<uint32_t> received_max_length;
  bool drop_callback = false;
  base::OnceClosure on_request;
};

class TestGetTextSurroundingSelectionFunction
    : public ExperimentalAiDataGetTextSurroundingSelectionFunction {
 public:
  void set_web_contents(content::WebContents* contents) {
    contents_ = contents;
  }

  int resolved_tab_id = -1;

 protected:
  ~TestGetTextSurroundingSelectionFunction() override = default;

  content::WebContents* GetTabById(int tab_id) override {
    resolved_tab_id = tab_id;
    return contents_;
  }

 private:
  raw_ptr<content::WebContents> contents_ = nullptr;
};

class ExperimentalAiDataSurroundingTextTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    main_local_frame_ =
        std::make_unique<FakeSurroundingTextLocalFrame>(main_rfh());
    NavigateAndCommit(GURL("https://example.test/"));
    extension_ = ExtensionBuilder("AI Data Extension")
                     .SetID("hpkopmikdojpadgmioifjjodbmnjjjca")
                     .AddAPIPermission("experimentalAiData")
                     .Build();
  }

  scoped_refptr<TestGetTextSurroundingSelectionFunction> CreateFunction() {
    auto function =
        base::MakeRefCounted<TestGetTextSurroundingSelectionFunction>();
    function->set_extension(extension_);
    function->set_web_contents(web_contents());
    return function;
  }

  void BlockHost(const char* pattern = "*://example.test/*") {
    extension_->permissions_data()->SetPolicyHostRestrictions(
        URLPatternSet({URLPattern(URLPattern::SCHEME_ALL, pattern)}), {});
  }

  std::optional<base::Value> Run(
      TestGetTextSurroundingSelectionFunction* function,
      const std::string& args) {
    return api_test_utils::RunFunctionAndReturnSingleResult(function, args,
                                                            profile());
  }

  std::string RunError(TestGetTextSurroundingSelectionFunction* function,
                       const std::string& args) {
    return api_test_utils::RunFunctionAndReturnError(function, args, profile());
  }

  std::unique_ptr<FakeSurroundingTextLocalFrame> main_local_frame_;
  ScopedCurrentChannel channel_{version_info::Channel::DEV};
  scoped_refptr<const Extension> extension_;
};

TEST_F(ExperimentalAiDataSurroundingTextTest,
       FetchesSurroundingTextFromMainFrameWhenUnfocused) {
  main_local_frame_->surrounding_text = u"Hello world surrounding text";
  main_local_frame_->start_offset = 6;
  main_local_frame_->end_offset = 11;

  auto function = CreateFunction();
  auto result = Run(function.get(), "[7, 50]");
  ASSERT_TRUE(result) << function->GetError();
  ASSERT_TRUE(result->is_dict());
  const auto& dict = result->GetDict();
  ASSERT_TRUE(dict.FindString("content"));
  EXPECT_EQ("Hello world surrounding text", *dict.FindString("content"));
  EXPECT_EQ(6, dict.FindInt("startOffset"));
  EXPECT_EQ(11, dict.FindInt("endOffset"));
  EXPECT_EQ(7, function->resolved_tab_id);
  EXPECT_EQ(50u, main_local_frame_->received_max_length);
}

TEST_F(ExperimentalAiDataSurroundingTextTest,
       FetchesSurroundingTextFromFocusedFrame) {
  auto* child =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  FakeSurroundingTextLocalFrame child_local_frame(child);
  child = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.test/subframe.html"), child);
  FocusWebContentsOnFrame(child);

  main_local_frame_->surrounding_text = u"Main frame text";
  child_local_frame.surrounding_text = u"Focused child frame text";
  child_local_frame.start_offset = 8;
  child_local_frame.end_offset = 13;

  auto function = CreateFunction();
  auto result = Run(function.get(), "[7, 25]");
  ASSERT_TRUE(result) << function->GetError();
  ASSERT_TRUE(result->is_dict());
  const auto& dict = result->GetDict();
  ASSERT_TRUE(dict.FindString("content"));
  EXPECT_EQ("Focused child frame text", *dict.FindString("content"));
  EXPECT_EQ(8, dict.FindInt("startOffset"));
  EXPECT_EQ(13, dict.FindInt("endOffset"));
  EXPECT_EQ(25u, child_local_frame.received_max_length);
  EXPECT_FALSE(main_local_frame_->received_max_length.has_value());
}

TEST_F(ExperimentalAiDataSurroundingTextTest, RejectsNegativeMaxLength) {
  auto function = CreateFunction();
  EXPECT_EQ("maxLength must not be negative.",
            RunError(function.get(), "[7, -1]"));
  EXPECT_FALSE(main_local_frame_->received_max_length.has_value());
}

TEST_F(ExperimentalAiDataSurroundingTextTest, RejectsInvalidTab) {
  auto function = CreateFunction();
  function->set_web_contents(nullptr);
  EXPECT_EQ("Invalid target tab passed in.",
            RunError(function.get(), "[7, 100]"));
  EXPECT_FALSE(main_local_frame_->received_max_length.has_value());
}

TEST_F(ExperimentalAiDataSurroundingTextTest, RejectsBlockedHost) {
  BlockHost();
  auto function = CreateFunction();
  EXPECT_EQ(kPolicyError, RunError(function.get(), "[7, 100]"));
  EXPECT_FALSE(main_local_frame_->received_max_length.has_value());
}

TEST_F(ExperimentalAiDataSurroundingTextTest,
       RejectsPageReplacementDuringRequest) {
  main_local_frame_->on_request = base::BindLambdaForTesting(
      [&] { NavigateAndCommit(GURL("https://other.test/")); });
  auto function = CreateFunction();
  EXPECT_EQ(kPageChangedError, RunError(function.get(), "[7, 100]"));
}

TEST_F(ExperimentalAiDataSurroundingTextTest,
       RejectsDroppedCallbackOnTabClosure) {
  auto function = CreateFunction();
  main_local_frame_->drop_callback = true;
  main_local_frame_->on_request = base::BindLambdaForTesting([&] {
    function->set_web_contents(nullptr);
    DeleteContents();
    main_local_frame_.reset();
  });
  EXPECT_EQ(kPageChangedError, RunError(function.get(), "[7, 100]"));
}

TEST_F(ExperimentalAiDataSurroundingTextTest,
       RejectsDroppedCallbackWithoutPageChange) {
  main_local_frame_->drop_callback = true;
  main_local_frame_->on_request =
      base::BindLambdaForTesting([&] { main_local_frame_.reset(); });
  auto function = CreateFunction();
  EXPECT_EQ("Data collection failed likely due to browser state change.",
            RunError(function.get(), "[7, 100]"));
}

}  // namespace
}  // namespace extensions
