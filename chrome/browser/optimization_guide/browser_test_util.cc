// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/browser_test_util.h"

#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test_utils.h"

namespace optimization_guide {

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run (such as chromeos) there
// might be two profiles created, and this will return the total sample count
// across profiles.
int GetTotalHistogramSamples(const base::HistogramTester* histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

}  // namespace

int RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    int total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;
  }
}

std::unique_ptr<optimization_guide::proto::GetModelsResponse>
BuildGetModelsResponse() {
  std::unique_ptr<optimization_guide::proto::GetModelsResponse>
      get_models_response =
          std::make_unique<optimization_guide::proto::GetModelsResponse>();

  optimization_guide::proto::PredictionModel* prediction_model =
      get_models_response->add_models();
  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(2);
  model_info->set_optimization_target(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  model_info->add_supported_model_engine_versions(
      optimization_guide::proto::ModelEngineVersion::
          MODEL_ENGINE_VERSION_TFLITE_2_8);
  prediction_model->mutable_model()->set_download_url(
      "https://example.com/model");

  return get_models_response;
}

void EnableSigninAndModelExecutionCapability(Profile* profile) {
  // Sign-in and enable account capability.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@example.com", signin::ConsentLevel::kSync);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

void EnableSigninWithoutModelExecutionCapability(Profile* profile) {
  // Sign-in and disable account capability.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@example.com", signin::ConsentLevel::kSync);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

ModelFileObserver::ModelFileObserver() = default;

ModelFileObserver::~ModelFileObserver() = default;

void ModelFileObserver::OnModelUpdated(
    proto::OptimizationTarget optimization_target,
    base::optional_ref<const ModelInfo> model_info) {
  optimization_target_ = optimization_target;
  model_info_ = std::nullopt;
  if (model_info.has_value()) {
    model_info_ = *model_info;
  }
  if (file_received_callback_)
    std::move(file_received_callback_).Run(optimization_target, model_info);
}

}  // namespace optimization_guide
