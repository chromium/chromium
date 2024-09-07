// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// See
// https://chromium.googlesource.com/chromium/src/+/main/docs/origin_trials_integration.md
const char kTestTokenPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";

const char kTrialEnabledDomain[] = "example.com";
const char kEmbeddingDomain[] = "embedding.com";
const char kFrobulatePersistentTrialName[] = "FrobulatePersistent";
// Generated with
// tools/origin_trials/generate_token.py https://example.com \
//     FrobulatePersistent --expire-timestamp=2000000000
const char kFrobulatePersistentToken[] =
    "AzZfd1vKZ0SSGRGk/"
    "8nIszQSlHYjbuYVE3jwaNZG3X4t11zRhzPWWJwTZ+JJDS3JJsyEZcpz+y20pAP6/"
    "6upOQ4AAABdeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI"
    "6ICJGcm9idWxhdGVQZXJzaXN0ZW50IiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";

const char kTrialEnabledPath[] = "/origin-trial";
const char kCriticalTrialEnabledPath[] = "/critical-origin-trial";

const char kPageWithOriginTrialResourcePath[] = "/has-origin-trial-resource";

const char kPageWithEmbeddedFramePath[] = "/has-embedded-frame";

const char kOriginTrialResourceJavascriptPath[] = "/origin-trial-script.js";

class OriginTrialsBrowserTest : public InProcessBrowserTest {
 public:
  OriginTrialsBrowserTest() {
    test_features_.InitAndEnableFeature(::features::kPersistentOriginTrials);
  }

  OriginTrialsBrowserTest(const OriginTrialsBrowserTest&) = delete;
  OriginTrialsBrowserTest& operator=(const OriginTrialsBrowserTest&) = delete;
  ~OriginTrialsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindRepeating(&OriginTrialsBrowserTest::InterceptRequest,
                            base::Unretained(this)));
  }

  bool InterceptRequest(content::URLLoaderInterceptor::RequestParams* params) {
    std::string path = params->url_request.url.path();

    std::string headers = "HTTP/1.1 200 OK\n";
    // Set Origin-Trial related headers
    if (params->url_request.url.DomainIs(kTrialEnabledDomain)) {
      received_request_counts_[path]++;
      if (path == kTrialEnabledPath || path == kCriticalTrialEnabledPath ||
          path == kOriginTrialResourceJavascriptPath) {
        base::StrAppend(&headers,
                        {"Origin-Trial: ", kFrobulatePersistentToken, "\n"});
      }
      if (path == kCriticalTrialEnabledPath) {
        base::StrAppend(
            &headers,
            {"Critical-Origin-Trial: ", kFrobulatePersistentTrialName, "\n"});
      }
    }

    // Set Content-Type header
    if (path == kOriginTrialResourceJavascriptPath) {
      base::StrAppend(&headers,
                      {"Content-Type: text/javascript; charset=utf-8\n"});
    } else {
      base::StrAppend(&headers, {"Content-Type: text/html; charset=utf-8\n"});
    }
    headers += '\n';

    // Set body contents
    std::string body;
    if (path == kPageWithOriginTrialResourcePath) {
      base::StrAppend(&body,
                      {"<!DOCTYPE html><head><script src=\"",
                       kOriginTrialResourceJavascriptPath, "\"></script>"});
    } else if (path == kPageWithEmbeddedFramePath) {
      base::StrAppend(&body, {"<!DOCTYPE html><body><iframe src=\"https://",
                              kTrialEnabledDomain, kCriticalTrialEnabledPath,
                              "\"></iframe>"});
    }

    content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                 params->client.get());

    return true;
  }

  void TearDownOnMainThread() override {
    // Clean up any saved settings after test run
    browser()
        ->profile()
        ->GetOriginTrialsControllerDelegate()
        ->ClearPersistedTokens();

    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  base::flat_set<std::string> GetOriginTrialsForEnabledOrigin(
      const std::string& partition_site) {
    url::Origin origin = url::Origin::CreateFromNormalizedTuple(
        "https", kTrialEnabledDomain, 443);
    url::Origin partition_domain =
        url::Origin::CreateFromNormalizedTuple("https", partition_site, 443);
    content::OriginTrialsControllerDelegate* delegate =
        browser()->profile()->GetOriginTrialsControllerDelegate();
    return delegate->GetPersistedTrialsForOrigin(origin, partition_domain,
                                                 base::Time::Now());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("origin-trial-public-key",
                                    kTestTokenPublicKey);
  }

  // Navigate to an insecure domain
  void RequestToHttpDomain() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("http://127.0.0.1/")));
  }

  // Navigate to our enabled origin without any Origin-Trial response headers
  void RequestWithoutHeaders() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(base::StrCat({"https://", kTrialEnabledDomain, "/"}))));
  }

  // Navigate to our enabled origin on a path that sets the Origin-Trial header.
  void RequestForOriginTrial(const std::string& path) {
    GURL url(base::StrCat({"https://", kTrialEnabledDomain, path}));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  // Navigate to a third-party page that embeds an origin trial-enabling page
  void RequestForEmbeddedOriginTrial() {
    GURL url(base::StrCat(
        {"https://", kEmbeddingDomain, kPageWithEmbeddedFramePath}));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

 protected:
  base::test::ScopedFeatureList test_features_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  base::flat_map<std::string, int> received_request_counts_;
};

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, NoHeaderDoesNotEnableResponse) {
  RequestWithoutHeaders();
  base::flat_set<std::string> trials =
      GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain);
  EXPECT_TRUE(trials.empty());
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest, ResponseEnablesOriginTrial) {
  RequestForOriginTrial(kTrialEnabledPath);
  base::flat_set<std::string> trials =
      GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain);
  ASSERT_FALSE(trials.empty());
  EXPECT_EQ(kFrobulatePersistentTrialName, *(trials.begin()));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       EmbeddedResponseEnablesPartitionedTrial) {
  RequestForEmbeddedOriginTrial();

  base::flat_set<std::string> trials =
      GetOriginTrialsForEnabledOrigin(kEmbeddingDomain);
  ASSERT_FALSE(trials.empty());
  EXPECT_EQ(kFrobulatePersistentTrialName, *(trials.begin()));

  ASSERT_TRUE(GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain).empty());
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       TrialEnabledAfterNavigationToOtherDomain) {
  // Navigate to a page that enables a persistent origin trial
  RequestForOriginTrial(kTrialEnabledPath);
  EXPECT_FALSE(GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain).empty());
  // Navigate to a different domain
  RequestToHttpDomain();

  // The trial should still be enabled
  base::flat_set<std::string> trials =
      GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain);
  ASSERT_FALSE(trials.empty());
  EXPECT_EQ(kFrobulatePersistentTrialName, *(trials.begin()));
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       TrialDisabledAfterNavigationToSameDomain) {
  // Navigate to a page that enables a persistent origin trial
  RequestForOriginTrial(kTrialEnabledPath);
  EXPECT_FALSE(GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain).empty());
  // Navigate to same domain without the Origin-Trial header set
  RequestWithoutHeaders();

  // The trial should no longer be enabled
  EXPECT_TRUE(GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain).empty());
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       CriticalOriginTrialRestartsRequest) {
  RequestForOriginTrial(kCriticalTrialEnabledPath);
  EXPECT_FALSE(GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain).empty());

  // The trial was critical, so expect two requests due to restart.
  EXPECT_EQ(2, received_request_counts_[kCriticalTrialEnabledPath]);

  // Navigate to another page
  RequestToHttpDomain();

  // Load the original page again
  received_request_counts_[kCriticalTrialEnabledPath] = 0;
  RequestForOriginTrial(kCriticalTrialEnabledPath);
  EXPECT_FALSE(GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain).empty());

  // The trial should already be persisted, so no restart should have happened
  EXPECT_EQ(1, received_request_counts_[kCriticalTrialEnabledPath]);
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       NonCriticalTrialDoesNotRestart) {
  RequestForOriginTrial(kTrialEnabledPath);
  EXPECT_FALSE(GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain).empty());

  // The trial was not critical, so expect one request.
  EXPECT_EQ(1, received_request_counts_[kTrialEnabledPath]);
}

IN_PROC_BROWSER_TEST_F(OriginTrialsBrowserTest,
                       OnlyNavigationRequestIsRestarted) {
  RequestForOriginTrial(kPageWithOriginTrialResourcePath);
  // We do not expect the trial to be set, since
  // |kPageWithOriginTrialResourcePath| doesn't set the header on navigation.
  EXPECT_TRUE(GetOriginTrialsForEnabledOrigin(kTrialEnabledDomain).empty());

  // The main page did not have any origin trial headers, so we only expect one
  // request.
  EXPECT_EQ(1, received_request_counts_[kPageWithOriginTrialResourcePath]);
  // Despite the javascript path setting headers, including
  // Critical-Origin-Trial, we do not expect a restart, as only navigations
  // should restart.
  EXPECT_EQ(1, received_request_counts_[kOriginTrialResourceJavascriptPath]);
}

}  // namespace
