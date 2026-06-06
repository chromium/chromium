// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_classifier.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/core/model_execution/test/substitution_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/classify_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/on_device_model/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_classifier.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

using ::base::test::TestFuture;
using ::optimization_guide::FieldSubstitution;
using ::optimization_guide::ForbidUnsafe;
using ::optimization_guide::ProtoField;
using ::optimization_guide::StringValueField;
using ::optimization_guide::proto::ClassifyApiRequest;
using ::optimization_guide::proto::ClassifyApiResponse;
using ::testing::_;
using ::testing::ElementsAreArray;

constexpr char kInputString[] = "input string";
constexpr char kContextString[] = "context string";

struct Error {
  blink::mojom::AIManagerCreateClientError error;
  blink::mojom::QuotaErrorInfoPtr quota_error_info;
};

using CreateClassifierResult =
    base::expected<mojo::PendingRemote<blink::mojom::AIClassifier>, Error>;

class TestCreateClassifierClient
    : public blink::mojom::AIManagerCreateClassifierClient {
 public:
  TestCreateClassifierClient() = default;
  ~TestCreateClassifierClient() override = default;
  TestCreateClassifierClient(const TestCreateClassifierClient&) = delete;
  TestCreateClassifierClient& operator=(const TestCreateClassifierClient&) =
      delete;

  mojo::PendingRemote<blink::mojom::AIManagerCreateClassifierClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnResult(
      mojo::PendingRemote<::blink::mojom::AIClassifier> classifier) override {
    result_.SetValue(std::move(classifier));
  }

  void OnError(blink::mojom::AIManagerCreateClientError error,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    result_.SetValue(
        base::unexpected(Error{error, std::move(quota_error_info)}));
  }

  TestFuture<CreateClassifierResult>& result() { return result_; }

 private:
  TestFuture<CreateClassifierResult> result_;
  mojo::Receiver<blink::mojom::AIManagerCreateClassifierClient> receiver_{this};
};

blink::mojom::AIClassifierCreateOptionsPtr GetDefaultOptions() {
  return blink::mojom::AIClassifierCreateOptions::New();
}

class AIClassifierTest : public AITestUtils::AITestBase {
 public:
  AIClassifierTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kAIClassifierAPI);
  }

 protected:
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig()
      override {
    optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_can_skip_text_safety(true);
    config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                           MODEL_EXECUTION_FEATURE_CLASSIFIER);

    auto& input_config = *config.mutable_input_config();
    input_config.set_request_base_name(ClassifyApiRequest().GetTypeName());

    *input_config.add_execute_substitutions() = FieldSubstitution(
        "%s", ProtoField({ClassifyApiRequest::kTextFieldNumber}));

    auto& output_config = *config.mutable_output_config();
    output_config.set_proto_type(ClassifyApiResponse().GetTypeName());
    *output_config.mutable_proto_field() = optimization_guide::ProtoField({1});

    return config;
  }

  mojo::Remote<blink::mojom::AIClassifier> GetAIClassifierRemote(
      blink::mojom::AIClassifierCreateOptionsPtr options =
          GetDefaultOptions()) {
    TestCreateClassifierClient create_classifier_client;
    GetAIManagerRemote()->CreateClassifier(
        create_classifier_client.BindNewPipeAndPassRemote(), std::move(options),
        /*monitor=*/mojo::NullRemote());

    CreateClassifierResult result = create_classifier_client.result().Take();
    EXPECT_OK(result);
    return mojo::Remote<blink::mojom::AIClassifier>(std::move(result.value()));
  }

  std::vector<std::string> Classify(blink::mojom::AIClassifier& classifier,
                                    const std::string& input,
                                    const std::string& context) {
    AITestUtils::TestStreamingResponder responder;
    classifier.Classify(input, context, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    return responder.responses_without_last();
  }

  void EnsureModelIsReady() {
    TestCreateClassifierClient classifier_client;
    GetAIManagerRemote()->CreateClassifier(
        classifier_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
        mojo::NullRemote());
    auto result = classifier_client.result().Take();
    EXPECT_TRUE(result.has_value());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AIClassifierTest, Classify) {
  fake_broker_->InstallClassifierModel(
      std::make_unique<optimization_guide::FakeBaseModelAsset>());
  fake_broker_->settings().set_execute_result({"Classified result"});

  mojo::Remote<blink::mojom::AIClassifier> classifier_remote =
      GetAIClassifierRemote();

  // The FakeBaseModelAsset/FakeService prepends "CPU backend".
  EXPECT_THAT(Classify(*classifier_remote, kInputString, kContextString),
              ElementsAreArray({"CPU backend", "Classified result"}));
}

TEST_F(AIClassifierTest, ClassifierTelemetry) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibility(
                  optimization_guide::mojom::OnDeviceFeature::kClassifier))
      .WillRepeatedly(testing::Return(
          optimization_guide::OnDeviceModelEligibilityReason::kSuccess));
  fake_broker_->InstallClassifierModel(
      std::make_unique<optimization_guide::FakeBaseModelAsset>());
  EnsureModelIsReady();
  GetAIClassifierRemote();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelEligibilityReason.Classifier",
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess, 2);
}

TEST_F(AIClassifierTest, CanCreateDefaultOptions) {
  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateClassifier(GetDefaultOptions(),
                                                 future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kDownloadable);
  }

  // After model is ready, `CanCreateClassifier` should return available.
  fake_broker_->InstallClassifierModel(
      std::make_unique<optimization_guide::FakeBaseModelAsset>());
  EnsureModelIsReady();

  {
    base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
    GetAIManagerInterface()->CanCreateClassifier(GetDefaultOptions(),
                                                 future.GetCallback());
    EXPECT_EQ(future.Get(),
              blink::mojom::ModelAvailabilityCheckResult::kAvailable);
  }
}

TEST_F(AIClassifierTest, CreateBuiltInAIAPIsEnterprisePolicyDisabled) {
  SetBuiltInAIAPIsEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateClassifier(GetDefaultOptions(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateClassifierClient create_classifier_client;
  GetAIManagerRemote()->CreateClassifier(
      create_classifier_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetBuiltInAIAPIsEnterprisePolicy(true);
}

TEST_F(AIClassifierTest, CreateGenAILocalEnterprisePolicyDisabled) {
  SetGenAILocalEnterprisePolicy(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateClassifier(GetDefaultOptions(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableEnterprisePolicyDisabled);

  mojo::test::BadMessageObserver observer;
  TestCreateClassifierClient create_classifier_client;
  GetAIManagerRemote()->CreateClassifier(
      create_classifier_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetGenAILocalEnterprisePolicy(true);
}

TEST_F(AIClassifierTest, CreateOnDeviceAiUserSettingDisabled) {
  SetOnDeviceAiUserSetting(false);
  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult> future;
  GetAIManagerInterface()->CanCreateClassifier(GetDefaultOptions(),
                                               future.GetCallback());
  EXPECT_EQ(future.Get(), blink::mojom::ModelAvailabilityCheckResult::
                              kUnavailableFeatureNotEnabled);

  mojo::test::BadMessageObserver observer;
  TestCreateClassifierClient create_classifier_client;
  GetAIManagerRemote()->CreateClassifier(
      create_classifier_client.BindNewPipeAndPassRemote(), GetDefaultOptions(),
      mojo::NullRemote());
  EXPECT_EQ(observer.WaitForBadMessage(), "Policy or user setting disabled");
  SetOnDeviceAiUserSetting(true);
}

}  // namespace
