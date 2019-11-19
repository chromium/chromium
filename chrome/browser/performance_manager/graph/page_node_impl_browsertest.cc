// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

constexpr char kOriginTrialTestPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

const char kTestHeaders[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";

constexpr char kOriginTrialTestHostname[] = "https://freeze_policy_ot_test.com";

// Origin Trial Token for PageFreezeOptOut generated with:
// $ tools/origin_trials/generate_token.py  \
//     https://freeze_policy_ot_test.com/  \
//     "PageFreezeOptOut"  \
//     --expire-timestamp=2000000000
// (Token will expire ca. ~2033. See content/test/data/origin_trials/basic.html)
constexpr char kOriginTrialPageFreezeOptOutToken[] =
    "ArK3cLVL1udOq9pE7armSlaBttK6PtImGGPbowl8jGzDaePbN9kIXgjd9s2UGW8J3UAYZp2/"
    "b0+"
    "dVRi2QeZAowQAAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9mcmVlemVfcG9saWN5X290X3Rlc3QuY"
    "29tOjQ0MyIsICJmZWF0dXJlIjogIlBhZ2VGcmVlemVPcHRPdXQiLCAiZXhwaXJ5IjogMjAwMDA"
    "wMDAwMH0=";

// Origin Trial Token for PageFreezeOptIn generated with:
// $ tools/origin_trials/generate_token.py  \
//     https://freeze_policy_ot_test.com/  \
//     "PageFreezeOptIn"  \
//     --expire-timestamp=2000000000
// (Token will expire ca. ~2033. See content/test/data/origin_trials/basic.html)
constexpr char kOriginTrialPageFreezeOptInToken[] =
    "AriV/"
    "ePX2G29BTjDZRzOvRCdkAeu34iLPHuelZxsgQiyJgJ3VADHBW2k1uFsgApSOEy4cEGdZ4hieud"
    "XKFYo7AsAAABneyJvcmlnaW4iOiAiaHR0cHM6Ly9mcmVlemVfcG9saWN5X290X3Rlc3QuY29tO"
    "jQ0MyIsICJmZWF0dXJlIjogIlBhZ2VGcmVlemVPcHRJbiIsICJleHBpcnkiOiAyMDAwMDAwMDA"
    "wfQ==";

// The path used in the URL of the Origin Trial Freeze Policy tests.
constexpr char kOriginTrialFreezePolicyTestPath[] = "ot_freeze_policy";

// The path used in the URL of the tests that uses 2 iFrames.
constexpr char k2iFramesPath[] = "two_iframe_tests";

// The page name used to set the appropriate Origin Trial tokens.
constexpr char kOriginTrialOptInPage[] = "optin.html";
constexpr char kOriginTrialOptOutPage[] = "optout.html";
constexpr char kOriginTrialDefaultPage[] = "default.html";

// Test pages that use 2 iFrames.
constexpr char kOriginTrialOptInOptOut[] = "optin_optout.html";
constexpr char kOriginTrialOptOutOptIn[] = "optout_optin.html";
constexpr char kOriginTrialDefaultOptIn[] = "default_optin.html";
constexpr char kOriginTrialDefaultOptOut[] = "default_optout.html";
constexpr char kOriginTrialOptInOptIn[] = "optin_optin.html";
constexpr char kOriginTrialOptOutOptOut[] = "optout_optout.html";
constexpr char kOriginTrialDefaultDefault[] = "default_default.html";

constexpr char kOriginTrialTestResponseTemplate[] = R"(
<html>
<head>
  <title>Freeze Policy test</title>
  META_TAG
</head>
</html>
)";

constexpr char kTwoiFrameTestBody[] = R"(
<html>
<base href="https://freeze_policy_ot_test.com/">
<head>
  <title>iFrame Test</title>
</head>
<iframe src="IFRAME_URL_1"></iframe>
<iframe src="IFRAME_URL_2"></iframe>
</html>
)";

// Generate a test response for the PageFreeze Origin Trial tests, based on
// kOriginTrialTestResponseTemplate.
std::string GetPageFreezeOriginTrialPageContent(const std::string& page) {
  std::string token;
  std::string contents = kOriginTrialTestResponseTemplate;

  // Get the appropriate Origin Trial token if needed.
  if (page.compare(kOriginTrialOptInPage) == 0) {
    token = kOriginTrialPageFreezeOptInToken;
  } else if (page.compare(kOriginTrialOptOutPage) == 0) {
    token = kOriginTrialPageFreezeOptOutToken;
  }

  std::string meta_tag;
  if (token.size()) {
    meta_tag.append(base::StrCat(
        {R"(<meta http-equiv="origin-trial" content=")", token, R"(">)"}));
  }
  base::ReplaceFirstSubstringAfterOffset(&contents, 0, "META_TAG", meta_tag);
  return contents;
}

std::string GetPageFreezeOriginTrialiFramePageContent(const std::string& page) {
  std::string contents = kTwoiFrameTestBody;
  std::string url1 = base::StrCat({kOriginTrialFreezePolicyTestPath, "/"});
  std::string url2 = base::StrCat({kOriginTrialFreezePolicyTestPath, "/"});

  std::string page_base_path = base::SplitString(
      page, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)[0];
  auto parts = base::SplitString(page_base_path, "_", base::KEEP_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  EXPECT_EQ(2U, parts.size());
  url1 += parts[0] + ".html";
  url2 += parts[1] + ".html";

  base::ReplaceFirstSubstringAfterOffset(&contents, 0, "IFRAME_URL_1", url1);
  base::ReplaceFirstSubstringAfterOffset(&contents, 0, "IFRAME_URL_2", url2);
  return contents;
}

// Generate the HTML content that should be served for |url|.
std::string GetContentForURL(const std::string& url) {
  if (!base::EndsWith(url, ".html", base::CompareCase::SENSITIVE))
    return std::string();

  auto url_parts = base::SplitString(url, "/", base::KEEP_WHITESPACE,
                                     base::SPLIT_WANT_NONEMPTY);
  // The URL should be composed of 2 parts, the base directory indicates the
  // kind of response to serve and the filename is passed to the function that
  // generates the response to serve the appropriate content.
  EXPECT_EQ(2U, url_parts.size());

  std::string response;
  if (url_parts[0].compare(kOriginTrialFreezePolicyTestPath) == 0) {
    response = GetPageFreezeOriginTrialPageContent(url_parts[1]);
  } else if (url_parts[0].compare(k2iFramesPath) == 0) {
    response = GetPageFreezeOriginTrialiFramePageContent(url_parts[1]);
  }
  return response;
}

void RunOriginTrialTestOnPMSequence(
    const mojom::InterventionPolicy expected_policy) {
  auto* perf_manager = PerformanceManagerImpl::GetInstance();
  ASSERT_TRUE(perf_manager);
  base::RunLoop run_loop;
  perf_manager->CallOnGraphImpl(
      FROM_HERE, base::BindOnce(
                     [](base::OnceClosure quit_closure,
                        const mojom::InterventionPolicy expected_policy,
                        performance_manager::GraphImpl* graph) {
                       auto page_nodes = graph->GetAllPageNodeImpls();
                       EXPECT_EQ(1U, page_nodes.size());
                       EXPECT_EQ(expected_policy,
                                 page_nodes[0]->origin_trial_freeze_policy());
                       std::move(quit_closure).Run();
                     },
                     run_loop.QuitClosure(), expected_policy));
  run_loop.Run();
}

bool URLLoaderInterceptorCallback(
    content::URLLoaderInterceptor::RequestParams* params) {
  content::URLLoaderInterceptor::WriteResponse(
      kTestHeaders, GetContentForURL(params->url_request.url.path()),
      params->client.get());

  return true;
}

}  // namespace

class PageNodeImplBrowserTest : public InProcessBrowserTest {
 public:
  PageNodeImplBrowserTest() = default;
  ~PageNodeImplBrowserTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindRepeating(&URLLoaderInterceptorCallback));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(PageNodeImplBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest, PageFreezeOriginTrialOptIn) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString(
                     {kOriginTrialTestHostname,
                      kOriginTrialFreezePolicyTestPath, kOriginTrialOptInPage},
                     "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kOptIn);
}

IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest, PageFreezeOriginTrialOptOut) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString(
                     {kOriginTrialTestHostname,
                      kOriginTrialFreezePolicyTestPath, kOriginTrialOptOutPage},
                     "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kOptOut);
}

IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest, PageFreezeOriginTrialDefault) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString({kOriginTrialTestHostname,
                                        kOriginTrialFreezePolicyTestPath,
                                        kOriginTrialDefaultPage},
                                       "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kDefault);
}

IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest,
                       PageFreezeOriginTrialOptInOptOut) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString({kOriginTrialTestHostname, k2iFramesPath,
                                        kOriginTrialOptInOptOut},
                                       "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kOptOut);
}

// Flaky. https://crbug.com/1014282
IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest,
                       DISABLED_PageFreezeOriginTrialOptOutOptIn) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString({kOriginTrialTestHostname, k2iFramesPath,
                                        kOriginTrialOptOutOptIn},
                                       "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kOptOut);
}

IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest,
                       PageFreezeOriginTrialDefaultOptIn) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString({kOriginTrialTestHostname, k2iFramesPath,
                                        kOriginTrialDefaultOptIn},
                                       "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kOptIn);
}

IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest,
                       PageFreezeOriginTrialDefaultOptOut) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString({kOriginTrialTestHostname, k2iFramesPath,
                                        kOriginTrialDefaultOptOut},
                                       "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kOptOut);
}

IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest,
                       PageFreezeOriginTrialOptInOptIn) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString({kOriginTrialTestHostname, k2iFramesPath,
                                        kOriginTrialOptInOptIn},
                                       "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kOptIn);
}

IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest,
                       PageFreezeOriginTrialOptOutOptOut) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString({kOriginTrialTestHostname, k2iFramesPath,
                                        kOriginTrialOptOutOptOut},
                                       "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kOptOut);
}

IN_PROC_BROWSER_TEST_F(PageNodeImplBrowserTest,
                       PageFreezeOriginTrialDefaultDefault) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(base::JoinString({kOriginTrialTestHostname, k2iFramesPath,
                                        kOriginTrialDefaultDefault},
                                       "/")));
  RunOriginTrialTestOnPMSequence(mojom::InterventionPolicy::kDefault);
}

// TODO(sebmarchand): Add more tests, e.g. a test where the main frame and a
// subframe set a different policy.

}  // namespace performance_manager
