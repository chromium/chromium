// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/services/machine_learning/metrics.h"
#include "chrome/services/machine_learning/public/cpp/service_connection.h"
#include "chrome/services/machine_learning/public/cpp/test_support/machine_learning_test_utils.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace machine_learning {
namespace {

class ServiceProcessObserver : public content::ServiceProcessHost::Observer {
 public:
  ServiceProcessObserver() { content::ServiceProcessHost::AddObserver(this); }
  ~ServiceProcessObserver() override {
    content::ServiceProcessHost::RemoveObserver(this);
  }

  ServiceProcessObserver(const ServiceProcessObserver&) = delete;
  ServiceProcessObserver& operator=(const ServiceProcessObserver&) = delete;

  // Whether the service is launched.
  int IsLaunched() const { return is_launched_; }

  // Whether the service is terminated normally.
  bool IsTerminated() const { return is_terminated_; }

  // Launch |launch_wait_loop_| to wait until a service launch is detected.
  void WaitForLaunch() { launch_wait_loop_.Run(); }

  // Launch |terminate_wait_loop_| to wait until a normal service termination is
  // detected.
  void WaitForTerminate() { terminate_wait_loop_.Run(); }

  void OnServiceProcessLaunched(
      const content::ServiceProcessInfo& info) override {
    if (info.IsService<mojom::MachineLearningService>()) {
      is_launched_ = true;
      if (launch_wait_loop_.running())
        launch_wait_loop_.Quit();
    }
  }

  void OnServiceProcessTerminatedNormally(
      const content::ServiceProcessInfo& info) override {
    if (info.IsService<mojom::MachineLearningService>()) {
      is_terminated_ = true;
      if (terminate_wait_loop_.running())
        terminate_wait_loop_.Quit();
    }
  }

 private:
  base::RunLoop launch_wait_loop_;
  base::RunLoop terminate_wait_loop_;
  bool is_launched_ = false;
  bool is_terminated_ = false;
};

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets)
      total_count += bucket.count;
    if (total_count >= count)
      return;

    content::FetchHistogramsFromChildProcesses();
    ::metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

using MachineLearningServiceBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MachineLearningServiceBrowserTest, LaunchAndTerminate) {
  base::HistogramTester histogram_tester;
  ServiceProcessObserver observer;
  auto* service_connection = ServiceConnection::GetInstance();

  service_connection->GetService();
  observer.WaitForLaunch();
  RetryForHistogramUntilCountReached(
      &histogram_tester, machine_learning::metrics::kServiceLaunch, 1);

  EXPECT_TRUE(service_connection);
  EXPECT_TRUE(observer.IsLaunched());

  histogram_tester.ExpectTotalCount(machine_learning::metrics::kServiceLaunch,
                                    1);
  histogram_tester.ExpectUniqueSample(
      machine_learning::metrics::kServiceRequested,
      machine_learning::metrics::MLServiceRequestStatus::
          kRequestedServiceNotLaunched,
      1);

  service_connection->ResetServiceForTesting();
  observer.WaitForTerminate();
  RetryForHistogramUntilCountReached(
      &histogram_tester, machine_learning::metrics::kServiceNormalTermination,
      1);

  EXPECT_TRUE(observer.IsTerminated());

  histogram_tester.ExpectTotalCount(
      machine_learning::metrics::kServiceNormalTermination, 1);
}

IN_PROC_BROWSER_TEST_F(MachineLearningServiceBrowserTest,
                       MultipleLaunchesReusesSharedProcess) {
  base::HistogramTester histogram_tester;
  ServiceProcessObserver observer;
  auto* service_connection = ServiceConnection::GetInstance();

  auto* service_ptr1 = service_connection->GetService();
  observer.WaitForLaunch();
  EXPECT_TRUE(service_connection);
  EXPECT_TRUE(observer.IsLaunched());

  RetryForHistogramUntilCountReached(
      &histogram_tester, machine_learning::metrics::kServiceLaunch, 1);
  histogram_tester.ExpectTotalCount(machine_learning::metrics::kServiceLaunch,
                                    1);
  histogram_tester.ExpectBucketCount(
      machine_learning::metrics::kServiceRequested,
      machine_learning::metrics::MLServiceRequestStatus::
          kRequestedServiceNotLaunched,
      1);

  auto* service_ptr2 = service_connection->GetService();
  EXPECT_EQ(service_ptr1, service_ptr2);

  RetryForHistogramUntilCountReached(
      &histogram_tester, machine_learning::metrics::kServiceRequested, 2);
  histogram_tester.ExpectTotalCount(machine_learning::metrics::kServiceLaunch,
                                    1);
  histogram_tester.ExpectBucketCount(
      machine_learning::metrics::kServiceRequested,
      machine_learning::metrics::MLServiceRequestStatus::
          kRequestedServiceLaunched,
      1);
}

IN_PROC_BROWSER_TEST_F(MachineLearningServiceBrowserTest,
                       LoadInvalidDecisionTreeModel) {
  base::HistogramTester histogram_tester;

  ServiceProcessObserver observer;
  auto run_loop = std::make_unique<base::RunLoop>();
  auto* service_connection = ServiceConnection::GetInstance();

  mojo::Remote<mojom::DecisionTreePredictor> predictor;
  mojom::LoadModelResult result = mojom::LoadModelResult::kLoadModelError;
  service_connection->LoadDecisionTreeModel(
      mojom::DecisionTreeModelSpec::New("Invalid model spec"),
      predictor.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](mojom::LoadModelResult* p_result, base::RunLoop* run_loop,
             mojom::LoadModelResult result) {
            *p_result = result;
            run_loop->Quit();
          },
          &result, run_loop.get()));
  run_loop->Run();
  EXPECT_TRUE(observer.IsLaunched());
  EXPECT_EQ(mojom::LoadModelResult::kModelSpecError, result);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      machine_learning::metrics::kDecisionTreeModelLoadResult, 1);
  histogram_tester.ExpectTotalCount(machine_learning::metrics::kServiceLaunch,
                                    1);
  histogram_tester.ExpectUniqueSample(
      machine_learning::metrics::kServiceRequested,
      machine_learning::metrics::MLServiceRequestStatus::
          kRequestedServiceNotLaunched,
      1);
  histogram_tester.ExpectUniqueSample(
      machine_learning::metrics::kDecisionTreeModelLoadResult,
      mojom::LoadModelResult::kModelSpecError, 1);

  // Flush so that |predictor| becomes aware of potential disconnection.
  predictor.FlushForTesting();
  EXPECT_FALSE(predictor.is_connected());
}

IN_PROC_BROWSER_TEST_F(MachineLearningServiceBrowserTest,
                       LoadValidDecisionTreeModelAndPredict) {
  base::HistogramTester histogram_tester;

  ServiceProcessObserver observer;
  auto* service_connection = ServiceConnection::GetInstance();
  const mojom::DecisionTreePredictionResult expected_prediction_result =
      mojom::DecisionTreePredictionResult::kTrue;

  auto model_proto =
      testing::GetModelProtoForPredictionResult(expected_prediction_result);
  mojo::Remote<mojom::DecisionTreePredictor> predictor;

  mojom::LoadModelResult load_result = mojom::LoadModelResult::kLoadModelError;
  auto run_loop = std::make_unique<base::RunLoop>();
  service_connection->LoadDecisionTreeModel(
      mojom::DecisionTreeModelSpec::New(model_proto->SerializeAsString()),
      predictor.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](mojom::LoadModelResult* p_result, base::RunLoop* run_loop,
             mojom::LoadModelResult result) {
            *p_result = result;
            run_loop->Quit();
          },
          &load_result, run_loop.get()));
  run_loop->Run();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      machine_learning::metrics::kDecisionTreeModelLoadResult, 1);

  EXPECT_TRUE(observer.IsLaunched());
  EXPECT_EQ(mojom::LoadModelResult::kOk, load_result);

  histogram_tester.ExpectTotalCount(machine_learning::metrics::kServiceLaunch,
                                    1);
  histogram_tester.ExpectTotalCount(
      machine_learning::metrics::kDecisionTreeModelValidationLatency, 1);
  histogram_tester.ExpectUniqueSample(
      machine_learning::metrics::kServiceRequested,
      machine_learning::metrics::MLServiceRequestStatus::
          kRequestedServiceNotLaunched,
      1);
  histogram_tester.ExpectUniqueSample(
      machine_learning::metrics::kDecisionTreeModelLoadResult,
      mojom::LoadModelResult::kOk, 1);

  // Flush so that |predictor| becomes aware of potential disconnection.
  predictor.FlushForTesting();
  EXPECT_TRUE(predictor.is_connected());

  auto prediction_result = mojom::DecisionTreePredictionResult::kUnknown;
  double prediction_score = 0.0;
  // Reset the RunLoop.
  run_loop = std::make_unique<base::RunLoop>();

  predictor->Predict(
      {}, base::BindOnce(
              [](mojom::DecisionTreePredictionResult* p_result, double* p_score,
                 base::RunLoop* runloop,
                 mojom::DecisionTreePredictionResult result, double score) {
                *p_result = result;
                *p_score = score;
                runloop->Quit();
              },
              &prediction_result, &prediction_score, run_loop.get()));
  run_loop->Run();
  predictor.FlushForTesting();
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      machine_learning::metrics::kDecisionTreeModelPredictionResult, 1);

  EXPECT_EQ(expected_prediction_result, prediction_result);
  EXPECT_GT(prediction_score, 0.0);

  histogram_tester.ExpectTotalCount(
      machine_learning::metrics::kDecisionTreeModelEvaluationLatency, 1);
  histogram_tester.ExpectUniqueSample(
      machine_learning::metrics::kDecisionTreeModelPredictionResult,
      prediction_result, 1);
}

}  // namespace machine_learning
