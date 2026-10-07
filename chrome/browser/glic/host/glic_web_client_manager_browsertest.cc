// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class GlicWebClientManagerBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest>,
      public testing::WithParamInterface<bool> {
 public:
  GlicWebClientManagerBrowserTest() {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;

    if (IsNoWebview()) {
      enabled.push_back(features::kGlicNoWebview);
    } else {
      disabled.push_back(features::kGlicNoWebview);
    }

    feature_list_.InitWithFeatures(enabled, disabled);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == "/_up") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content("<html><body>Login</body></html>");
            response->set_content_type("text/html");
            return response;
          }
          return nullptr;
        }));
    GlicBrowserTestMixin<PlatformBrowserTest>::SetUpOnMainThread();
  }

  bool IsNoWebview() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       TimeToWarmedRecordedOnClientLoad) {
  base::HistogramTester histogram_tester;

  ASSERT_OK(OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient());

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 1);
  auto samples = histogram_tester.GetAllSamples("Glic.Contents.TimeToWarmed");
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_GT(samples[0].min, 0);
}

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       TimeToWarmedRecordedOnceOnly) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 1);

  // Subsequent open calls for another tab do not re-record TimeToWarmed for
  // the existing web client.
  CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK(OpenGlicForActiveTab());
  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 1);

  // Subsequent state event (kWarmed) or receiver creation on the manager does
  // not re-record TimeToWarmed because it has already been recorded.
  GlicWebClientManager* manager =
      instance->host().GetWebClientManagerForTesting();
  ASSERT_TRUE(manager);
  manager->OnWebClientStateChangedForTesting(mojom::WebClientState::kWarmed);

  mojo::PendingRemote<glic::mojom::WebClientHandler> remote;
  manager->SetPendingWebClientReceiver(remote.InitWithNewPipeAndPassReceiver());

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 1);
}

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       TimeToWarmedNotRecordedOnNonRegularNavigation) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<content::WebContents> test_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  GlicWebClientManager manager;
  manager.AttachGuestContents(test_contents.get());

  // Navigating to a login URL (e.g. /_up) classifies the page
  // as non-regular (kLogin), disqualifying it from TimeToWarmed recording.
  ASSERT_TRUE(content::NavigateToURL(test_contents.get(),
                                     embedded_test_server()->GetURL("/_up")));

  manager.OnWebClientStateChangedForTesting(mojom::WebClientState::kWarmed);
  mojo::PendingRemote<glic::mojom::WebClientHandler> remote;
  manager.SetPendingWebClientReceiver(remote.InitWithNewPipeAndPassReceiver());

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 0);
}

IN_PROC_BROWSER_TEST_P(GlicWebClientManagerBrowserTest,
                       TimeToWarmedNotRecordedOnCrash) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<content::WebContents> test_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  GlicWebClientManager manager;
  manager.AttachGuestContents(test_contents.get());

  // Navigate to an initial regular page so a render process host exists.
  ASSERT_TRUE(content::NavigateToURL(
      test_contents.get(), embedded_test_server()->GetURL("/title1.html")));

  // A render process crash disqualifies the contents from TimeToWarmed
  // recording.
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    content::CrashTab(test_contents.get());
  }

  manager.OnWebClientStateChangedForTesting(mojom::WebClientState::kWarmed);
  mojo::PendingRemote<glic::mojom::WebClientHandler> remote;
  manager.SetPendingWebClientReceiver(remote.InitWithNewPipeAndPassReceiver());

  histogram_tester.ExpectTotalCount("Glic.Contents.TimeToWarmed", 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicWebClientManagerBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

}  // namespace
}  // namespace glic
