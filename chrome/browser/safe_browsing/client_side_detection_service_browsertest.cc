// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_service.h"

#include <optional>

#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_client_side_detection_service_delegate.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
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
    if (callback_) {
      std::move(callback_).Run();
    }
  }

 private:
  mojo::Receiver<mojom::PhishingModelSetterTestObserver> receiver_;
  base::OnceClosure callback_;
};

}  // namespace

using ::testing::_;
using ::testing::ReturnRef;
using ::testing::StrictMock;

class ClientSideDetectionServiceBrowserTest : public PlatformBrowserTest {
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

  void LoadVisualTfLiteModel() {
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    content::RenderProcessHost* rph = rfh->GetProcess();

    std::unique_ptr<PhishingModelWaiter> waiter =
        CreatePhishingModelWaiter(rph);

    base::RunLoop run_loop;
    waiter->SetCallback(run_loop.QuitClosure());
    safe_browsing::ClientSideDetectionService* csd_service =
        ClientSideDetectionServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    base::FilePath model_file_path;
    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_TEST_DATA, &model_file_path));
    model_file_path = model_file_path.AppendASCII("safe_browsing")
                          .AppendASCII("client_model.pb");

    base::FilePath additional_files_path;
    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_TEST_DATA, &additional_files_path));

#if BUILDFLAG(IS_ANDROID)
    additional_files_path = additional_files_path.AppendASCII("safe_browsing")
                                .AppendASCII("visual_model_android.tflite");
#else
    additional_files_path = additional_files_path.AppendASCII("safe_browsing")
                                .AppendASCII("visual_model_desktop.tflite");
#endif
    csd_service->SetModelAndVisualTfLiteForTesting(model_file_path,
                                                   additional_files_path);
    run_loop.Run();
  }

  ClientPhishingRequest CreateMinimumClientPhishingRequest() {
    ClientPhishingRequest request;
    safe_browsing::ClientSideDetectionService* csd_service =
        ClientSideDetectionServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    size_t thresholds_size =
        csd_service->GetVisualTfLiteModelThresholds().size();
    for (size_t i = 0; i < thresholds_size; i++) {
      ClientPhishingRequest::CategoryScore* category =
          request.add_tflite_model_scores();
      category->set_label("dummy_label");
      category->set_value(0.0);
    }
    return request;
  }
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionServiceBrowserTest,
                       ModelUpdatesPropagated) {
  base::HistogramTester histogram_tester;
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Bucket 0 is success, no direct enum reference call due to illegal include.
  histogram_tester.ExpectBucketCount(
      "SBClientPhishing.FlatBufferScorer.CreationStatus", 0, 0);

  LoadVisualTfLiteModel();

  // Because the histogram data from renderer takes time to propagate to the
  // browser process, this function must be called to reduce flakiness.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Check that the update was successful. The histogram value can flakily range
  // from 1-3 depending on platform, hence the EXPECT_GE is used.
  EXPECT_GE(histogram_tester.GetBucketCount(
                "SBClientPhishing.FlatBufferScorer.CreationStatus", 0),
            0);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionServiceBrowserTest,
                       NoImageEmbeddingMatch) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  LoadVisualTfLiteModel();

  // Setup TargetEmbeddings.
  float threshold = 0.95;
  tflite::task::vision::FeatureVector target_fv;
  for (int i = 0; i < 64; ++i) {
    target_fv.add_value_float(i / 100.0);
  }
  TargetEmbedding target(target_fv, threshold);
  std::vector<TargetEmbedding> test_targets;
  test_targets.push_back(std::move(target));

  safe_browsing::ClientSideDetectionService* csd_service =
      ClientSideDetectionServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  csd_service->SetTargetImageEmbeddingsForTesting(std::move(test_targets));

  // Case 1: ClientPhishingRequest's image embedding doesn't meet threshold.
  ClientPhishingRequest request = CreateMinimumClientPhishingRequest();

  // Add an image_feature_embedding that will match.
  auto* features = request.mutable_image_feature_embedding();
  for (int i = 0; i < 64; ++i) {
    features->add_embedding_value(100);  // No match.
  }

  // Call the classification function.
  csd_service->ClassifyPhishingThroughThresholds(&request);

  EXPECT_FALSE(request.is_phishing());
  ASSERT_FALSE(request.has_target_image_embedding_score());

  // Case 2: Visual TFLite already flagged the page phishy.
  ClientPhishingRequest request2 = CreateMinimumClientPhishingRequest();

  // Add an image_feature_embedding that will match.
  auto* features2 = request2.mutable_image_feature_embedding();
  for (int i = 0; i < 64; ++i) {
    features2->add_embedding_value(i / 100.0);  // Perfect match.
  }

  // Preemptively set the phishing to true.
  request2.set_is_phishing(true);

  // Call the classification function.
  csd_service->ClassifyPhishingThroughThresholds(&request2);

  EXPECT_TRUE(request2.is_phishing());
  ASSERT_FALSE(request2.has_target_image_embedding_score());  // Not set despite
                                                              // perfect match.
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionServiceBrowserTest,
                       ImageEmbeddingMatch) {
  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  safe_browsing::ClientSideDetectionService* csd_service =
      ClientSideDetectionServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  LoadVisualTfLiteModel();

  ClientPhishingRequest request = CreateMinimumClientPhishingRequest();

  // Setup TargetEmbeddings.
  float threshold = 0.95;
  tflite::task::vision::FeatureVector target_fv;
  for (int i = 0; i < 64; ++i) {
    target_fv.add_value_float(0.1);
  }
  TargetEmbedding target(target_fv, threshold);
  std::vector<TargetEmbedding> test_targets;
  test_targets.push_back(std::move(target));
  csd_service->SetTargetImageEmbeddingsForTesting(std::move(test_targets));

  // Add an image_feature_embedding that will match.
  auto* features = request.mutable_image_feature_embedding();
  for (int i = 0; i < 63; ++i) {
    features->add_embedding_value(0.1);
  }
  features->add_embedding_value(0.1000001);  // Near Perfect match.

  // Call the classification function.
  csd_service->ClassifyPhishingThroughThresholds(&request);

  EXPECT_TRUE(request.is_phishing());
  ASSERT_TRUE(request.has_target_image_embedding_score());
  EXPECT_EQ(request.target_image_embedding_score().id(),
            "ac7d206f95fb23a5ad4d52d76c4acd22d16bdcdbb3a6decc66f8eaabc9b40534");
  EXPECT_NEAR(request.target_image_embedding_score().score(), 1.0, 0.001);
}

}  // namespace safe_browsing
