// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/proto/loading_predictor_metadata.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class LoadingPredictorExtensionBrowserTest : public ExtensionBrowserTest {
 public:
  LoadingPredictorExtensionBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kLoadingPredictorUseOptimizationGuide,
          {{"use_predictions", "true"},
           {"always_retrieve_predictions", "true"}}},
         {features::kLoadingPredictorPrefetch, {}}},
        {});
  }
  ~LoadingPredictorExtensionBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
    ExtensionBrowserTest::SetUpOnMainThread();
  }

  void AddOptimizationGuidePrediction(GURL main_frame_url,
                                      GURL subresource_url) {
    auto* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide::proto::LoadingPredictorMetadata
        loading_predictor_metadata;

    optimization_guide::proto::Resource* subresource =
        loading_predictor_metadata.add_subresources();
    subresource->set_url(subresource_url.spec());
    subresource->set_resource_type(
        optimization_guide::proto::ResourceType::RESOURCE_TYPE_SCRIPT);
    subresource->set_preconnect_only(false);

    optimization_guide::OptimizationMetadata optimization_metadata;
    optimization_metadata.set_loading_predictor_metadata(
        loading_predictor_metadata);
    optimization_guide_keyed_service->AddHintForTesting(
        main_frame_url, optimization_guide::proto::LOADING_PREDICTOR,
        optimization_metadata);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a prefetch triggered by LoadingPredictor is seen by an extension
// (via webRequest).
IN_PROC_BROWSER_TEST_F(LoadingPredictorExtensionBrowserTest,
                       PrefetchObservedByExtension) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("predictors").AppendASCII("web_request"));
  ASSERT_TRUE(extension);

  GURL main_frame_url =
      embedded_https_test_server().GetURL("a.com", "/title1.html");
  GURL subresource_url =
      embedded_https_test_server().GetURL("b.com", "/predictor/empty.js");

  AddOptimizationGuidePrediction(main_frame_url, subresource_url);

  content::RenderFrameHost* rfh =
      ui_test_utils::NavigateToURL(browser(), main_frame_url);
  ASSERT_EQ(rfh->GetLastCommittedURL(), main_frame_url);
  ASSERT_FALSE(rfh->IsErrorDocument());

  EXPECT_EQ(ExecuteScriptInBackgroundPage(
                extension->id(),
                content::JsReplace("wasRequestSeen($1);", subresource_url)),
            true);

  EXPECT_EQ(ExecuteScriptInBackgroundPage(
                extension->id(),
                content::JsReplace("getInitiator($1)", subresource_url)),
            url::Origin::Create(main_frame_url).Serialize());

  EXPECT_EQ(ExecuteScriptInBackgroundPage(
                extension->id(),
                content::JsReplace("getRequestType($1)", subresource_url)),
            "script");
}

// Tests that a prefetch triggered by LoadingPredictor is blocked by an
// extension using declarativeNetRequest.
IN_PROC_BROWSER_TEST_F(LoadingPredictorExtensionBrowserTest,
                       PrefetchBlockedByExtension) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("predictors")
                        .AppendASCII("declarative_net_request"));
  ASSERT_TRUE(extension);

  GURL main_frame_url =
      embedded_https_test_server().GetURL("a.com", "/title1.html");
  GURL subresource_url =
      embedded_https_test_server().GetURL("b.com", "/predictor/empty.js");

  AddOptimizationGuidePrediction(main_frame_url, subresource_url);

  content::RenderFrameHost* rfh =
      ui_test_utils::NavigateToURL(browser(), main_frame_url);
  ASSERT_EQ(rfh->GetLastCommittedURL(), main_frame_url);
  ASSERT_FALSE(rfh->IsErrorDocument());

  EXPECT_EQ(
      ExecuteScriptInBackgroundPage(
          extension->id(),
          content::JsReplace("waitUntilRequestBlocked($1);", subresource_url)),
      true);
}

}  // namespace extensions
