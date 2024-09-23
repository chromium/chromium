// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

namespace {

// Generate token with the command:
// tools/origin_trials/generate_token.py https://example.test
// PermissionElement
// --expire-days 5000
constexpr std::string_view kOriginTrialToken =
    "AyhOIRw/ha6vGsSq2BU78sDZ49hP+Cv6OC191Ae7YQHf3pYW8UJ5bwCOOuUXjfA/"
    "QmXR6y1+4cv+"
    "Uy6utB3FJw0AAABceyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYX"
    "R1cmUiOiAiUGVybWlzc2lvbkVsZW1lbnQiLCAiZXhwaXJ5IjogMjE0NzE1NjYyOH0=";

enum class BaseFeatureStatus {
  kDisabled,
  kEnabled,
  kDefault,
};

enum class BlinkFeatureStatus {
  kDisabled,
  kEnabled,
  kDefault,
};

std::string BaseFeatureStatusToString(const BaseFeatureStatus& type) {
  switch (type) {
    case BaseFeatureStatus::kDisabled:
      return "BaseDisabled";
    case BaseFeatureStatus::kEnabled:
      return "BaseEnabled";
    case BaseFeatureStatus::kDefault:
      return "BaseDefault";
  }
}

std::string BlinkFeatureStatusToString(const BlinkFeatureStatus& type) {
  switch (type) {
    case BlinkFeatureStatus::kDisabled:
      return "BlinkDisabled";
    case BlinkFeatureStatus::kEnabled:
      return "BlinkEnabled";
    case BlinkFeatureStatus::kDefault:
      return "BlinkDefault";
  }
}

std::string FeaturesStatusesToString(
    const testing::TestParamInfo<
        testing::tuple<BaseFeatureStatus, BlinkFeatureStatus>>& info) {
  return BaseFeatureStatusToString(testing::get<0>(info.param)) + "_" +
         BlinkFeatureStatusToString(testing::get<1>(info.param));
}

}  // namespace

class PermissionElementOriginTrialBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          testing::tuple<BaseFeatureStatus, BlinkFeatureStatus>> {
 protected:
  PermissionElementOriginTrialBrowserTest() {
    switch (testing::get<0>(GetParam())) {
      case BaseFeatureStatus::kDisabled:
        feature_list_.InitAndDisableFeature(
            blink::features::kPermissionElement);
        break;
      case BaseFeatureStatus::kEnabled:
        feature_list_.InitAndEnableFeature(blink::features::kPermissionElement);
        break;
      case BaseFeatureStatus::kDefault:
        break;
    }
  }
  ~PermissionElementOriginTrialBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // Add the public key following:
    // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/origin_trials_integration.md#manual-testing.
    command_line->AppendSwitchASCII(
        "origin-trial-public-key",
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=");
    switch (testing::get<1>(GetParam())) {
      case BlinkFeatureStatus::kDisabled:
        command_line->AppendSwitchASCII(switches::kDisableBlinkFeatures,
                                        "PermissionElement");
        break;
      case BlinkFeatureStatus::kEnabled:
        command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                        "PermissionElement");
        break;
      case BlinkFeatureStatus::kDefault:
        break;
    }
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    url_loader_interceptor_.emplace(base::BindRepeating(
        &PermissionElementOriginTrialBrowserTest::InterceptRequest,
        base::Unretained(this)));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  void LoadPage(bool with_origin_trial_token) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        with_origin_trial_token ? GetUrl("/origin_trial") : GetUrl("/")));
  }

  void CheckFeatureEnabled(bool enable) {
    EXPECT_EQ(enable, HasPermissionElement());
    if (enable) {
      EXPECT_TRUE(
          base::FeatureList::IsEnabled(blink::features::kPermissionElement));
    }
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  static GURL GetUrl(const std::string& path) {
    return GURL("https://example.test:443/").Resolve(path);
  }

  bool InterceptRequest(content::URLLoaderInterceptor::RequestParams* params) {
    // Setting up origin trial header.
    std::string headers =
        "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
    if (params->url_request.url.path() == "/origin_trial") {
      base::StrAppend(&headers, {"Origin-Trial: ", kOriginTrialToken, "\n"});
    }
    headers += '\n';
    content::URLLoaderInterceptor::WriteResponse(headers, "",
                                                 params->client.get());
    return true;
  }

  bool HasPermissionElement() {
    return content::EvalJs(GetActiveWebContents(),
                           "typeof HTMLPermissionElement === 'function'")
        .ExtractBool();
  }

  std::optional<content::URLLoaderInterceptor> url_loader_interceptor_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PermissionElementOriginTrialBrowserTest,
    testing::Combine(testing::Values(BaseFeatureStatus::kDisabled,
                                     BaseFeatureStatus::kEnabled,
                                     BaseFeatureStatus::kDefault),
                     testing::Values(BlinkFeatureStatus::kDisabled,
                                     BlinkFeatureStatus::kEnabled,
                                     BlinkFeatureStatus::kDefault)),
    FeaturesStatusesToString);

IN_PROC_BROWSER_TEST_P(PermissionElementOriginTrialBrowserTest,
                       WithoutOriginTrialToken) {
  LoadPage(/*with_origin_trial_token*/ false);
  switch (testing::get<0>(GetParam())) {
    case BaseFeatureStatus::kDisabled:
      CheckFeatureEnabled(/*enable*/ false);
      break;
    case BaseFeatureStatus::kEnabled:
      CheckFeatureEnabled(testing::get<1>(GetParam()) !=
                          BlinkFeatureStatus::kDisabled);
      break;
    case BaseFeatureStatus::kDefault:
      CheckFeatureEnabled(testing::get<1>(GetParam()) ==
                          BlinkFeatureStatus::kEnabled);
      break;
  }
}

IN_PROC_BROWSER_TEST_P(PermissionElementOriginTrialBrowserTest,
                       WithOriginTrialToken) {
  LoadPage(/*with_origin_trial_token*/ true);
  switch (testing::get<0>(GetParam())) {
    case BaseFeatureStatus::kDisabled:
      CheckFeatureEnabled(/*enable*/ false);
      break;
    case BaseFeatureStatus::kEnabled:
      [[fallthrough]];
    case BaseFeatureStatus::kDefault:
      CheckFeatureEnabled(/*enable*/ true);
      break;
  }
}
