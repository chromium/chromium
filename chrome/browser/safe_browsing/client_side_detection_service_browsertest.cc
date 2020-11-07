// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_service.h"

#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "content/public/test/browser_test.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

using ::testing::_;
using ::testing::ReturnRef;
using ::testing::StrictMock;

namespace {

class FakeModelLoader : public ModelLoader {
 public:
  explicit FakeModelLoader(std::string model_str)
      : ModelLoader(base::RepeatingClosure(),
                    nullptr,
                    /*is_extended_reporting=*/false) {
    model_str_ = model_str;
  }
  ~FakeModelLoader() override = default;

  void ScheduleFetch(int64_t delay) override {}
  void CancelFetcher() override {}
};

std::unique_ptr<ModelLoader> CreateFakeModelLoader(std::string model_str) {
  return std::make_unique<FakeModelLoader>(model_str);
}

}  // namespace

class ClientSideDetectionServiceBrowserTest : public InProcessBrowserTest {
  void SetUpOnMainThread() override {}
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionServiceBrowserTest,
                       NewHostGetsModel) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);
  ClientSideDetectionService* csd_service =
      ClientSideDetectionServiceFactory::GetForProfile(browser()->profile());

  ClientSideModel model;
  model.set_max_words_per_term(0);
  std::string model_str;
  model.SerializeToString(&model_str);

  csd_service->SetModelLoaderFactoryForTesting(
      base::BindRepeating(&CreateFakeModelLoader, model_str));

  // Enable Safe Browsing and the CSD service.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               true);

  base::RunLoop run_loop;

  content::RenderFrameHost* rfh =
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  mojo::Remote<mojom::PhishingDetector> phishing_detector;
  rfh->GetRemoteInterfaces()->GetInterface(
      phishing_detector.BindNewPipeAndPassReceiver());

  mojom::PhishingDetectorResult result;
  std::string verdict;
  phishing_detector->StartPhishingDetection(
      GURL("about:blank"),
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

  // The model classification will run, but will return an invalid score.
  EXPECT_EQ(result, mojom::PhishingDetectorResult::INVALID_SCORE);
}

}  // namespace safe_browsing
