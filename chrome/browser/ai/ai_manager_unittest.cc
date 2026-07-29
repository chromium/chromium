// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/task/current_thread.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ai/ai_language_model.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/work_in_progress.mojom.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_rewriter.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_summarizer.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_writer.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/component_updater/ai_embeddings_component_installer.h"
#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"
#endif

using optimization_guide::MockSession;

using testing::_;
using testing::AtMost;
using testing::NiceMock;

namespace {

class AISemanticEmbedderServiceLauncherForTest
    : public AISemanticEmbedderServiceLauncher {};

std::vector<blink::mojom::AILanguageCodePtr> MakeLanguageCodeVector(
    const std::vector<std::string>& languages) {
  std::vector<blink::mojom::AILanguageCodePtr> result;
  for (const auto& language : languages) {
    result.push_back(blink::mojom::AILanguageCode::New(language));
  }
  return result;
}

class TestCreateSemanticEmbedderClient
    : public blink::mojom::AIManagerCreateSemanticEmbedderClient {
 public:
  TestCreateSemanticEmbedderClient() = default;
  ~TestCreateSemanticEmbedderClient() override = default;

  void OnResult(
      mojo::PendingRemote<blink::mojom::AISemanticEmbedder> embedder) override {
    future_.SetValue(std::move(embedder));
  }

  void OnError(blink::mojom::AIManagerCreateClientError error) override {
    error_future_.SetValue(error);
  }

  mojo::PendingRemote<blink::mojom::AIManagerCreateSemanticEmbedderClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  base::test::TestFuture<mojo::PendingRemote<blink::mojom::AISemanticEmbedder>>&
  future() {
    return future_;
  }

  base::test::TestFuture<blink::mojom::AIManagerCreateClientError>&
  error_future() {
    return error_future_;
  }

 private:
  mojo::Receiver<blink::mojom::AIManagerCreateSemanticEmbedderClient> receiver_{
      this};
  base::test::TestFuture<mojo::PendingRemote<blink::mojom::AISemanticEmbedder>>
      future_;
  base::test::TestFuture<blink::mojom::AIManagerCreateClientError>
      error_future_;
};

class MockDownloadObserver : public on_device_model::mojom::DownloadObserver {
 public:
  MockDownloadObserver() = default;
  ~MockDownloadObserver() override = default;

  MOCK_METHOD(void,
              OnDownloadProgressUpdate,
              (uint64_t downloaded_bytes, uint64_t total_bytes),
              (override));

  mojo::PendingRemote<on_device_model::mojom::DownloadObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<on_device_model::mojom::DownloadObserver> receiver_{this};
};

#if !BUILDFLAG(IS_ANDROID)
class MockOnDemandUpdater : public component_updater::OnDemandUpdater {
 public:
  MockOnDemandUpdater() = default;
  ~MockOnDemandUpdater() override = default;

  MOCK_METHOD(void,
              OnDemandUpdate,
              (const std::string&,
               component_updater::OnDemandUpdater::Priority,
               component_updater::Callback),
              (override));
};
#endif

class AIManagerTest : public AITestUtils::AITestBase {
 public:
  AIManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kAIPromptAPI, blink::features::kAIWriterAPI,
         blink::features::kAISummarizationAPI, blink::features::kAIRewriterAPI,
         blink::features::kAIProofreadingAPI,
         blink::features::kAIEmbeddingsAPI},
        {});
  }

  void SetUp() override {
    AITestUtils::AITestBase::SetUp();
    launcher_ = std::make_unique<AISemanticEmbedderServiceLauncherForTest>();
    AISemanticEmbedderServiceLauncher::SetForTesting(launcher_.get());
  }

  void TearDown() override {
    AISemanticEmbedderServiceLauncher::SetForTesting(nullptr);
    launcher_.reset();
#if !BUILDFLAG(IS_ANDROID)
    fake_component_updater_ptr_ = nullptr;
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(nullptr);
#endif
    AITestUtils::AITestBase::TearDown();
  }

 protected:
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_can_skip_text_safety(true);
    config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                           MODEL_EXECUTION_FEATURE_PROMPT_API);
    return config;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AISemanticEmbedderServiceLauncherForTest> launcher_;

#if !BUILDFLAG(IS_ANDROID)
 public:
  void SetupFakeComponentUpdater() {
    auto fake_component_updater =
        std::make_unique<optimization_guide::FakeComponentUpdateService>();
    fake_component_updater_ptr_ = fake_component_updater.get();
    EXPECT_CALL(*fake_component_updater_ptr_, GetOnDemandUpdater())
        .WillRepeatedly(testing::ReturnRef(mock_on_demand_updater_));
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(
        std::move(fake_component_updater));
  }

 protected:
  NiceMock<MockOnDemandUpdater> mock_on_demand_updater_;
  raw_ptr<optimization_guide::FakeComponentUpdateService>
      fake_component_updater_ptr_;
#endif
};

// Tests that involve invalid on-device model file paths should not crash when
// the associated RFH is destroyed.
TEST_F(AIManagerTest, NoUAFWithInvalidOnDeviceModelPath) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      optimization_guide::switches::kOnDeviceModelExecutionOverride,
      "invalid-on-device-model-file-path");

  base::MockCallback<blink::mojom::AIManager::CanCreateLanguageModelCallback>
      callback;
  EXPECT_CALL(callback, Run(_)).Times(AtMost(1));
  ai_manager_->CanCreateLanguageModel(/*options=*/{}, callback.Get());

  // The callback may still be pending, delete the WebContents and destroy the
  // associated RFH, which should not result in a UAF.
  DeleteContents();

  task_environment()->RunUntilIdle();
}

TEST_F(AIManagerTest, CanCreate) {
  // Model is not downloaded until first session is created, so `CanCreate`
  // returns `kDownloadable`.
  // Android hasn't implement other APIs beside CanCreateSummarizer, so only
  // test CanCreateSummarizer.
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateSummarizer(/*options=*/{}, future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }
#if !BUILDFLAG(IS_ANDROID)
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateLanguageModel(/*options=*/{}, future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateWriter(/*options=*/{}, future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateRewriter(/*options=*/{}, future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

TEST_F(AIManagerTest, CanCreateSemanticEmbedderCrashLimit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kAIEmbeddingsAPI},
      /*disabled_features=*/{});

  auto* service_launcher = AISemanticEmbedderServiceLauncher::Get();
  service_launcher->RecordSuccessfulUse();
  service_launcher->controller()->MaybeUpdateModelInfo(
      optimization_guide::ModelInfo{
          .model_file_path = base::FilePath(FILE_PATH_LITERAL("embeddings")),
          .additional_files = {base::FilePath(FILE_PATH_LITERAL("sp"))},
          .version = 1,
          .model_metadata = optimization_guide::AnyWrapProto(
              optimization_guide::proto::PassageEmbeddingsModelMetadata()),
      });

  // Ensure it's ready.
  EXPECT_TRUE(service_launcher->controller()->IsModelAvailable());

  // Check it is available
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateSemanticEmbedder(future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }

  // Crash 3 times.
  service_launcher->OnServiceDisconnected(false);
  service_launcher->OnServiceDisconnected(false);
  service_launcher->OnServiceDisconnected(false);

  // Check it is unavailable.
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateSemanticEmbedder(future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableTooManyRecentCrashes);
  }

  // Cleanup
  service_launcher->RecordSuccessfulUse();
  service_launcher->controller()->MaybeUpdateModelInfo(std::nullopt);
}

TEST_F(AIManagerTest, CanCreateNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          optimization_guide::features::kOptimizationGuideModelExecution,
          blink::features::kAIEmbeddingsAPI});
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateLanguageModel(/*options=*/{}, future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableFeatureNotEnabled);
  }
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateWriter(/*options=*/{}, future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableFeatureNotEnabled);
  }
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateSummarizer(/*options=*/{}, future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableFeatureNotEnabled);
  }
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateRewriter(/*options=*/{}, future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableFeatureNotEnabled);
  }
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    ai_manager_->CanCreateSemanticEmbedder(future.GetCallback());
    EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableFeatureNotEnabled);
  }
}

TEST_F(AIManagerTest, CanCreateFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {},
      {blink::features::kAIPromptAPI,
       blink::features::kAIPromptAPIMultimodalInput,
       blink::features::kAIWriterAPI, blink::features::kAISummarizationAPI,
       blink::features::kAIRewriterAPI, blink::features::kAIProofreadingAPI});

  base::MockCallback<
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)>>
      callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableFeatureNotEnabled))
      .Times(5);

  ai_manager_->CanCreateLanguageModel(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateWriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSummarizer(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateRewriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateProofreader(/*options=*/{}, callback.Get());
}

TEST_F(AIManagerTest, CanCreateEnterprisePolicyDisabled) {
  SetBuiltInAIAPIsEnterprisePolicy(false);
  base::MockCallback<
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)>>
      callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableEnterprisePolicyDisabled))
      .Times(6);

  ai_manager_->CanCreateLanguageModel(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateWriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSummarizer(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateRewriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateProofreader(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSemanticEmbedder(callback.Get());
  SetBuiltInAIAPIsEnterprisePolicy(true);
}

TEST_F(AIManagerTest, CanCreateLocalStateEnterprisePolicyDisabled) {
  SetGenAILocalEnterprisePolicy(false);
  base::MockCallback<
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)>>
      callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableEnterprisePolicyDisabled))
      .Times(6);

  ai_manager_->CanCreateLanguageModel(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateWriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSummarizer(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateRewriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateProofreader(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSemanticEmbedder(callback.Get());
  SetGenAILocalEnterprisePolicy(true);
}

TEST_F(AIManagerTest, CanCreateLocalStateUserSettingsDisabled) {
  SetOnDeviceAiUserSetting(false);
  base::MockCallback<
      base::OnceCallback<void(blink::mojom::ModelAvailabilityCheckResult)>>
      callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableFeatureNotEnabled))
      .Times(6);

  ai_manager_->CanCreateLanguageModel(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateWriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSummarizer(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateRewriter(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateProofreader(/*options=*/{}, callback.Get());
  ai_manager_->CanCreateSemanticEmbedder(callback.Get());
  SetOnDeviceAiUserSetting(true);
}

// Test CheckAndFixLanguages templates for LanguageModel.
TEST_F(AIManagerTest, CheckAndFixLanguagesLanguageModel) {
  base::flat_set<std::string> enabled = {"en", "es", "ja"};
  auto make_expected = [](const base::flat_set<std::string>& languages) {
    auto expected = blink::mojom::AILanguageModelExpected::New();
    expected->languages.emplace();
    for (const auto& language : languages) {
      expected->languages->push_back(
          blink::mojom::AILanguageCode::New(language));
    }
    return expected;
  };

  auto make_options = [&](const base::flat_set<std::string>& inputs,
                          const base::flat_set<std::string>& outputs) {
    auto options = blink::mojom::AILanguageModelCreateOptions::New();
    options->expected_inputs.emplace();
    options->expected_inputs->push_back(make_expected(inputs));
    options->expected_outputs.emplace();
    options->expected_outputs->push_back(make_expected(outputs));
    return options;
  };

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  options = make_options({"en", "es-MX"}, {});
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  options = make_options({}, {"en-UK", "es-SP", "ja-JP"});
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  options = make_options({"en", "fr"}, {});
  EXPECT_FALSE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  options = make_options({"en"}, {"hi"});
  EXPECT_FALSE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
}

// Test CheckAndFixLanguages templates for Summarizer, Writer, and Rewriter.
TEST_F(AIManagerTest, CheckAndFixLanguagesWritingAssistance) {
  base::flat_set<std::string> enabled = {"en", "es", "ja"};
  auto make_options = [](const std::vector<std::string>& input,
                         const std::vector<std::string>& context,
                         const std::string& output) {
    auto options = blink::mojom::AISummarizerCreateOptions::New();
    options->expected_input_languages = MakeLanguageCodeVector(input);
    options->expected_context_languages = MakeLanguageCodeVector(context);
    options->output_language = blink::mojom::AILanguageCode::New(output);
    return options;
  };

  auto options = blink::mojom::AISummarizerCreateOptions::New();
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  options = make_options({}, {}, "");
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  EXPECT_TRUE(options->output_language->code.empty());
  options = make_options({"en", "es-MX"}, {"ja"}, "en-US");
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  options = make_options({"en-UK", "en-US"}, {"en"}, "");
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  EXPECT_EQ(options->output_language->code, "en-UK");
  options = make_options({"en", "fr"}, {}, "hi");
  EXPECT_FALSE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
}

// Test CheckAndFixLanguages templates for Proofreader.
TEST_F(AIManagerTest, CheckAndFixLanguagesProofreader) {
  base::flat_set<std::string> enabled = {"en", "es", "ja"};
  auto make_options = [](const std::vector<std::string>& input,
                         const std::string& correction_explanation) {
    auto options = blink::mojom::AIProofreaderCreateOptions::New();
    options->expected_input_languages = MakeLanguageCodeVector(input);
    options->correction_explanation_language =
        blink::mojom::AILanguageCode::New(correction_explanation);
    return options;
  };

  auto options = blink::mojom::AIProofreaderCreateOptions::New();
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  options = make_options({}, "");
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  EXPECT_TRUE(options->correction_explanation_language->code.empty());
  options = make_options({"en", "es-MX", "ja"}, "en-US");
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  options = make_options({"en-UK", "en-US", "en"}, "");
  EXPECT_TRUE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
  EXPECT_EQ(options->correction_explanation_language->code, "en-UK");
  options = make_options({"en", "fr"}, "hi");
  EXPECT_FALSE(
      ai_manager_->CheckAndFixLanguages(options, "API", enabled, enabled));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(AIManagerTest, CreateSemanticEmbedderWaitsForModel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kAIEmbeddingsAPI},
      /*disabled_features=*/{});

  SetupFakeComponentUpdater();

  auto* service_launcher = AISemanticEmbedderServiceLauncher::Get();
  service_launcher->RecordSuccessfulUse();

  // Model is not yet available.
  EXPECT_FALSE(service_launcher->controller()->IsModelAvailable());

  TestCreateSemanticEmbedderClient client;
  MockDownloadObserver monitor;

  ai_manager_->CreateSemanticEmbedder(client.BindNewPipeAndPassRemote(),
                                      monitor.BindNewPipeAndPassRemote());

  // Wait a bit and verify it hasn't resolved.
  task_environment()->RunUntilIdle();  // nocheck
  EXPECT_FALSE(client.future().IsReady());

  // Now provide the model.
  service_launcher->controller()->MaybeUpdateModelInfo(
      optimization_guide::ModelInfo{
          .model_file_path = base::FilePath(FILE_PATH_LITERAL("embeddings")),
          .additional_files = {base::FilePath(FILE_PATH_LITERAL("sp"))},
          .version = 1,
          .model_metadata = optimization_guide::AnyWrapProto(
              optimization_guide::proto::PassageEmbeddingsModelMetadata()),
      });

  // It should now be resolved.
  EXPECT_TRUE(client.future().Wait());
}

TEST_F(AIManagerTest, CreateSemanticEmbedderDownloadProgress) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      optimization_guide::features::kAIModelUnloadableProgress);

  SetupFakeComponentUpdater();

  auto* service_launcher = AISemanticEmbedderServiceLauncher::Get();
  service_launcher->RecordSuccessfulUse();

  TestCreateSemanticEmbedderClient client;
  MockDownloadObserver monitor;

  EXPECT_CALL(monitor,
              OnDownloadProgressUpdate(
                  0, optimization_guide::kNormalizedDownloadProgressMax));
  // 500 out of 1000 bytes corresponds to 50% progress, and 50% of
  // optimization_guide::kNormalizedDownloadProgressMax (65536) equals 32768.
  EXPECT_CALL(monitor,
              OnDownloadProgressUpdate(
                  32768, optimization_guide::kNormalizedDownloadProgressMax));
  EXPECT_CALL(monitor, OnDownloadProgressUpdate(
                           optimization_guide::kNormalizedDownloadProgressMax,
                           optimization_guide::kNormalizedDownloadProgressMax));

  ai_manager_->CreateSemanticEmbedder(client.BindNewPipeAndPassRemote(),
                                      monitor.BindNewPipeAndPassRemote());

  task_environment()->RunUntilIdle();  // nocheck
  EXPECT_FALSE(client.future().IsReady());

  optimization_guide::FakeComponent component(
      component_updater::GetAIEmbeddingsComponentId(), /*total_bytes=*/1000);

  fake_component_updater_ptr_->SendUpdate(component.CreateUpdateItem(
      update_client::ComponentState::kDownloading, 0));
  task_environment()->FastForwardBy(base::Milliseconds(51));

  fake_component_updater_ptr_->SendUpdate(component.CreateUpdateItem(
      update_client::ComponentState::kDownloading, 500));
  task_environment()->FastForwardBy(base::Milliseconds(51));

  fake_component_updater_ptr_->SendUpdate(component.CreateUpdateItem(
      update_client::ComponentState::kDownloading, 1000));
  task_environment()->FastForwardBy(base::Milliseconds(51));

  service_launcher->controller()->MaybeUpdateModelInfo(
      optimization_guide::ModelInfo{
          .model_file_path = base::FilePath(FILE_PATH_LITERAL("embeddings")),
          .additional_files = {base::FilePath(FILE_PATH_LITERAL("sp"))},
          .version = 1,
          .model_metadata = optimization_guide::AnyWrapProto(
              optimization_guide::proto::PassageEmbeddingsModelMetadata()),
      });

  EXPECT_TRUE(client.future().Wait());
}

TEST_F(AIManagerTest, CreateSemanticEmbedderCrashLimit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kAIEmbeddingsAPI},
      /*disabled_features=*/{});

  auto* service_launcher = AISemanticEmbedderServiceLauncher::Get();

  // Simulate consecutive crashes.
  service_launcher->OnServiceDisconnected(/*is_idle=*/false);
  service_launcher->OnServiceDisconnected(/*is_idle=*/false);
  service_launcher->OnServiceDisconnected(/*is_idle=*/false);

  EXPECT_FALSE(service_launcher->AllowedToLaunch());

  TestCreateSemanticEmbedderClient client;
  MockDownloadObserver monitor;

  ai_manager_->CreateSemanticEmbedder(client.BindNewPipeAndPassRemote(),
                                      monitor.BindNewPipeAndPassRemote());

  EXPECT_TRUE(client.error_future().Wait());
  EXPECT_EQ(client.error_future().Get(),
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AIManagerTest, CreateSemanticEmbedderComponentUpdateFailed) {
  SetupFakeComponentUpdater();

  auto* service_launcher = AISemanticEmbedderServiceLauncher::Get();

  // Model is not yet available.
  EXPECT_FALSE(service_launcher->controller()->IsModelAvailable());

  TestCreateSemanticEmbedderClient client;
  MockDownloadObserver monitor;

  // Expect OnDemandUpdate and capture the callback.
  component_updater::Callback on_demand_callback;
  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(
                  component_updater::GetAIEmbeddingsComponentId(),
                  component_updater::OnDemandUpdater::Priority::FOREGROUND, _))
      .WillOnce(
          [&on_demand_callback](const std::string&,
                                component_updater::OnDemandUpdater::Priority,
                                component_updater::Callback callback) {
            on_demand_callback = std::move(callback);
          });

  ai_manager_->CreateSemanticEmbedder(client.BindNewPipeAndPassRemote(),
                                      monitor.BindNewPipeAndPassRemote());

  task_environment()->RunUntilIdle();  // nocheck
  EXPECT_FALSE(client.future().IsReady());
  EXPECT_FALSE(client.error_future().IsReady());

  // Now execute the callback with an error.
  ASSERT_FALSE(on_demand_callback.is_null());
  std::move(on_demand_callback).Run(update_client::Error::SERVICE_ERROR);

  // It should now be failed and client should receive OnError.
  EXPECT_TRUE(client.error_future().Wait());
  EXPECT_EQ(client.error_future().Get(),
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
