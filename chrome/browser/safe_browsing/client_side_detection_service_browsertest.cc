// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_service_delegate.h"

#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#endif  // defined (

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace safe_browsing {

namespace {

// Helper class used to wait until a phishing model has been set.
class PhishingModelWaiter : public mojom::PhishingModelSetterTestObserver {
 public:
  explicit PhishingModelWaiter(
      mojo::PendingReceiver<mojom::PhishingModelSetterTestObserver> receiver)
      : receiver_(this, std::move(receiver)) {}

  void SetCallback(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

  // mojom::PhishingModelSetterTestObserver
  void PhishingModelUpdated() override {
    if (callback_)
      std::move(callback_).Run();
  }

 private:
  mojo::Receiver<mojom::PhishingModelSetterTestObserver> receiver_;
  base::OnceClosure callback_;
};

}  // namespace

using ::testing::_;
using ::testing::ReturnRef;
using ::testing::StrictMock;

class ClientSideDetectionServiceBrowserTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ClientSideDetectionServiceBrowserTest() {
    if (ShouldEnableCacao()) {
      feature_list_.InitAndEnableFeature(
          kClientSideDetectionModelOptimizationGuide);
    }
  }

  bool ShouldEnableCacao() { return GetParam(); }

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  std::unique_ptr<PhishingModelWaiter> CreatePhishingModelWaiter(
      content::RenderProcessHost* rph) {
    mojo::AssociatedRemote<mojom::PhishingModelSetter> model_setter;
    rph->GetChannel()->GetRemoteAssociatedInterface(&model_setter);

    mojo::PendingRemote<mojom::PhishingModelSetterTestObserver> observer;
    auto waiter = std::make_unique<PhishingModelWaiter>(
        observer.InitWithNewPipeAndPassReceiver());

    {
      base::RunLoop run_loop;
      model_setter->SetTestObserver(std::move(observer),
                                    run_loop.QuitClosure());
      run_loop.Run();
    }

    return waiter;
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClientSideDetectionServiceBrowserTest,
                         testing::Bool());

// TODO(crbug.com/1434848): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG)
#define MAYBE_ModelUpdatesPropagated DISABLED_ModelUpdatesPropagated
#else
#define MAYBE_ModelUpdatesPropagated ModelUpdatesPropagated
#endif
IN_PROC_BROWSER_TEST_P(ClientSideDetectionServiceBrowserTest,
                       MAYBE_ModelUpdatesPropagated) {
#if BUILDFLAG(IS_MAC)
  if (base::mac::IsAtLeastOS13()) {
    GTEST_SKIP() << "Flaky on macOS 13: https://crbug.com/1433315";
  }
#endif
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  content::RenderProcessHost* rph = rfh->GetProcess();

  // Update the model and wait for confirmation
  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::unique_ptr<PhishingModelWaiter> waiter =
        CreatePhishingModelWaiter(rph);

    base::RunLoop run_loop;
    waiter->SetCallback(run_loop.QuitClosure());

    if (base::FeatureList::IsEnabled(
            kClientSideDetectionModelOptimizationGuide)) {
      safe_browsing::ClientSideDetectionService* csd_service =
          ClientSideDetectionServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
      base::FilePath model_file_path;
      ASSERT_TRUE(
          base::PathService::Get(chrome::DIR_TEST_DATA, &model_file_path));
      model_file_path = model_file_path.AppendASCII("safe_browsing")
                            .AppendASCII("client_model.pb");

      base::FilePath additional_files_path;
      ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA,
                                         &additional_files_path));

#if BUILDFLAG(IS_ANDROID)
      additional_files_path = additional_files_path.AppendASCII("safe_browsing")
                                  .AppendASCII("visual_model_android.tflite");
#else
      additional_files_path = additional_files_path.AppendASCII("safe_browsing")
                                  .AppendASCII("visual_model_desktop.tflite");
#endif
      csd_service->SetModelAndVisualTfLiteForTesting(model_file_path,
                                                     additional_files_path);
    } else {
      ClientSideModel model;
      model.set_version(123);
      model.set_max_words_per_term(0);
      std::string model_str;
      model.SerializeToString(&model_str);

      ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
          CSDModelType::kProtobuf);
      ClientSidePhishingModel::GetInstance()->SetModelStrForTesting(model_str);
      ClientSidePhishingModel::GetInstance()
          ->NotifyCallbacksOfUpdateForTesting();
    }
    run_loop.Run();
  }

  // Check that the update was successful
  {
    base::RunLoop run_loop;

    mojo::AssociatedRemote<mojom::PhishingDetector> phishing_detector;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&phishing_detector);

    mojom::PhishingDetectorResult result;
    std::string verdict;
    phishing_detector->StartPhishingDetection(
        url,
        base::BindOnce(
            [](base::RepeatingClosure quit_closure,
               mojom::PhishingDetectorResult* out_result,
               std::string* out_verdict, mojom::PhishingDetectorResult result,
               const std::string& verdict) {
              *out_result = result;
              *out_verdict = verdict;
              quit_closure.Run();
            },
            run_loop.QuitClosure(), &result, &verdict));

    run_loop.Run();

    EXPECT_EQ(result, mojom::PhishingDetectorResult::SUCCESS);

    ClientPhishingRequest request;
    ASSERT_TRUE(request.ParseFromString(verdict));
    if (!base::FeatureList::IsEnabled(
            kClientSideDetectionModelOptimizationGuide)) {
      EXPECT_EQ(123, request.model_version());
    } else {
      EXPECT_EQ(27, request.model_version());  // Example model file version
    }
  }
}

// TODO(crbug.com/1434848): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG)
#define MAYBE_TfLiteClassification DISABLED_TfLiteClassification
#else
#define MAYBE_TfLiteClassification TfLiteClassification
#endif
IN_PROC_BROWSER_TEST_P(ClientSideDetectionServiceBrowserTest,
                       MAYBE_TfLiteClassification) {
#if BUILDFLAG(IS_MAC)
  if (base::mac::IsAtLeastOS13()) {
    GTEST_SKIP() << "Flaky on macOS 13: https://crbug.com/1433315";
  }
#endif

  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  content::RenderProcessHost* rph = rfh->GetProcess();

  base::HistogramTester histogram_tester;

  safe_browsing::ClientSideDetectionService* csd_service =
      ClientSideDetectionServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  // Update the model and wait for confirmation
  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::unique_ptr<PhishingModelWaiter> waiter =
        CreatePhishingModelWaiter(rph);

    base::RunLoop run_loop;
    waiter->SetCallback(run_loop.QuitClosure());

    if (!base::FeatureList::IsEnabled(
            kClientSideDetectionModelOptimizationGuide)) {
      ClientSideModel model;
      model.set_version(123);
      model.set_max_words_per_term(0);

      model.mutable_tflite_metadata()->set_input_width(48);
      model.mutable_tflite_metadata()->set_input_height(48);

      std::vector<std::pair<std::string, double>> thresholds{
          {"502fd246eb6fad3eae0387c54e4ebe74", 2.0},
          {"7c4065b088444b37d273872b771e6940", 2.0},
          {"712036bd72bf185a2a4f88de9141d02d", 2.0},
          {"9e9c15bfa7cb3f8699e2271116a4175c", 2.0},
          {"6c2cb3f559e7a03f37dd873fc007dc65", 2.0},
          {"1cbeb74661a5e7e05c993f2524781611", 2.0},
          {"989790016b6adca9d46b9c8ec6b8fe3a", 2.0},
          {"501067590331ca2d243c669e6084c47e", 2.0},
          {"40aed7e33c100058e54c73af3ed49524", 2.0},
          {"62f53ea23c7ad2590db711235a45fd38", 2.0},
          {"ee6fb9baa44f192bc3c53d8d3c6f7a3d", 2.0},
          {"ea54b0830d871286e2b4023bbb431710", 2.0},
          {"25645a55b844f970337218ea8f1f26b7", 2.0},
          {"c9a8640be09f97f170f1a2708058c48f", 2.0},
          {"953255ea26aa8578d06593ff33e99298", 2.0}};
      for (const auto& label_and_threshold : thresholds) {
        TfLiteModelMetadata::Threshold* threshold =
            model.mutable_tflite_metadata()->add_thresholds();
        threshold->set_label(label_and_threshold.first);
        threshold->set_threshold(label_and_threshold.second);
      }

      std::string model_str;
      model.SerializeToString(&model_str);
      ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
          CSDModelType::kProtobuf);
      ClientSidePhishingModel::GetInstance()->SetModelStrForTesting(model_str);
      ClientSidePhishingModel::GetInstance()
          ->NotifyCallbacksOfUpdateForTesting();

      run_loop.Run();
    } else {
      base::FilePath tflite_path;
      ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &tflite_path));
#if BUILDFLAG(IS_ANDROID)
      tflite_path = tflite_path.AppendASCII("safe_browsing")
                        .AppendASCII("visual_model_android.tflite");
#else
      tflite_path = tflite_path.AppendASCII("safe_browsing")
                        .AppendASCII("visual_model_desktop.tflite");
#endif
      base::File tflite_model(tflite_path,
                              base::File::FLAG_OPEN | base::File::FLAG_READ);
      ASSERT_TRUE(tflite_model.IsValid());

      base::FilePath model_file_path;
      ASSERT_TRUE(
          base::PathService::Get(chrome::DIR_TEST_DATA, &model_file_path));
      model_file_path = model_file_path.AppendASCII("safe_browsing")
                            .AppendASCII("client_model.pb");

      csd_service->SetModelAndVisualTfLiteForTesting(model_file_path,
                                                     tflite_path);
      run_loop.Run();
    }
  }

  // Check that the update was successful
  {
    base::RunLoop run_loop;

    mojo::AssociatedRemote<mojom::PhishingDetector> phishing_detector;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&phishing_detector);

    mojom::PhishingDetectorResult result;
    std::string verdict;
    phishing_detector->StartPhishingDetection(
        url,
        base::BindOnce(
            [](base::RepeatingClosure quit_closure,
               mojom::PhishingDetectorResult* out_result,
               std::string* out_verdict, mojom::PhishingDetectorResult result,
               const std::string& verdict) {
              *out_result = result;
              *out_verdict = verdict;
              quit_closure.Run();
            },
            run_loop.QuitClosure(), &result, &verdict));

    run_loop.Run();

    EXPECT_EQ(result, mojom::PhishingDetectorResult::SUCCESS);

    ClientPhishingRequest request;
    ASSERT_TRUE(request.ParseFromString(verdict));
    if (!base::FeatureList::IsEnabled(
            kClientSideDetectionModelOptimizationGuide)) {
      EXPECT_EQ(123, request.model_version());
    } else {
      EXPECT_EQ(27, request.model_version());
      csd_service->ClassifyPhishingThroughThresholds(&request);

      histogram_tester.ExpectUniqueSample(
          "SBClientPhishing.ClassifyThresholdsResult",
          safe_browsing::SBClientDetectionClassifyThresholdsResult::kSuccess,
          1);  // Example model file version
    }
  }
}

// TODO(crbug.com/1434848): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG)
#define MAYBE_TfLiteClassificationAfterTwoModelUploads \
  DISABLED_TfLiteClassificationAfterTwoModelUploads
#else
#define MAYBE_TfLiteClassificationAfterTwoModelUploads \
  TfLiteClassificationAfterTwoModelUploads
#endif
IN_PROC_BROWSER_TEST_P(ClientSideDetectionServiceBrowserTest,
                       MAYBE_TfLiteClassificationAfterTwoModelUploads) {
#if BUILDFLAG(IS_MAC)
  if (base::mac::IsAtLeastOS13()) {
    GTEST_SKIP() << "Flaky on macOS 13: https://crbug.com/1433315";
  }
#endif

  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    return;
  }
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
  content::RenderProcessHost* rph = rfh->GetProcess();

  base::HistogramTester histogram_tester;

  safe_browsing::ClientSideDetectionService* csd_service =
      ClientSideDetectionServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  // Update the model and wait for confirmation
  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::unique_ptr<PhishingModelWaiter> waiter =
        CreatePhishingModelWaiter(rph);

    base::RunLoop run_loop;
    waiter->SetCallback(run_loop.QuitClosure());

    base::FilePath tflite_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &tflite_path));
#if BUILDFLAG(IS_ANDROID)
    tflite_path = tflite_path.AppendASCII("safe_browsing")
                      .AppendASCII("visual_model_android.tflite");
#else
    tflite_path = tflite_path.AppendASCII("safe_browsing")
                      .AppendASCII("visual_model_desktop.tflite");
#endif
    base::File tflite_model(tflite_path,
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(tflite_model.IsValid());

    base::FilePath model_file_path;
    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_TEST_DATA, &model_file_path));
    model_file_path = model_file_path.AppendASCII("safe_browsing")
                          .AppendASCII("client_model.pb");
    csd_service->SetModelAndVisualTfLiteForTesting(model_file_path,
                                                   tflite_path);

    run_loop.Run();
  }

  size_t first_thresholds_map_size =
      csd_service->GetVisualTfLiteModelThresholds().size();

  // Second time
  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::unique_ptr<PhishingModelWaiter> waiter =
        CreatePhishingModelWaiter(rph);

    base::RunLoop run_loop;
    waiter->SetCallback(run_loop.QuitClosure());

    base::FilePath tflite_path;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &tflite_path));
#if BUILDFLAG(IS_ANDROID)
    tflite_path = tflite_path.AppendASCII("safe_browsing")
                      .AppendASCII("visual_model_android.tflite");
#else
    tflite_path = tflite_path.AppendASCII("safe_browsing")
                      .AppendASCII("visual_model_desktop.tflite");
#endif
    base::File tflite_model(tflite_path,
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(tflite_model.IsValid());

    base::FilePath model_file_path;
    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_TEST_DATA, &model_file_path));
    model_file_path = model_file_path.AppendASCII("safe_browsing")
                          .AppendASCII("client_model.pb");

    csd_service->SetModelAndVisualTfLiteForTesting(model_file_path,
                                                   tflite_path);
    run_loop.Run();
  }

  EXPECT_EQ(first_thresholds_map_size,
            csd_service->GetVisualTfLiteModelThresholds().size());

  // Check that the update was successful
  {
    base::RunLoop run_loop;

    mojo::AssociatedRemote<mojom::PhishingDetector> phishing_detector;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&phishing_detector);

    mojom::PhishingDetectorResult result;
    std::string verdict;
    phishing_detector->StartPhishingDetection(
        url,
        base::BindOnce(
            [](base::RepeatingClosure quit_closure,
               mojom::PhishingDetectorResult* out_result,
               std::string* out_verdict, mojom::PhishingDetectorResult result,
               const std::string& verdict) {
              *out_result = result;
              *out_verdict = verdict;
              quit_closure.Run();
            },
            run_loop.QuitClosure(), &result, &verdict));

    run_loop.Run();

    EXPECT_EQ(result, mojom::PhishingDetectorResult::SUCCESS);

    ClientPhishingRequest request;
    ASSERT_TRUE(request.ParseFromString(verdict));
    if (!base::FeatureList::IsEnabled(
            kClientSideDetectionModelOptimizationGuide)) {
      EXPECT_EQ(123, request.model_version());
    } else {
      EXPECT_EQ(27, request.model_version());  // Example model file version

      csd_service->ClassifyPhishingThroughThresholds(&request);

      histogram_tester.ExpectUniqueSample(
          "SBClientPhishing.ClassifyThresholdsResult",
          safe_browsing::SBClientDetectionClassifyThresholdsResult::kSuccess,
          1);
    }
  }
}

}  // namespace safe_browsing
