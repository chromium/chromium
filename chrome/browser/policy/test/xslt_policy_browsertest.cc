// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/features.h"

namespace policy {

enum class Policy {
  kDefault,
  kTrue,
  kFalse,
};

enum class FeatureState {
  kEnabled,
  kDisabled,
};

class XSLTPolicyBrowserTest : public PolicyTest,
                              public ::testing::WithParamInterface<
                                  std::tuple<Policy, FeatureState, bool>> {
 public:
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    std::string description;
    switch (std::get<0>(info.param)) {
      case Policy::kDefault:
        description += "PolicyDefault";
        break;
      case Policy::kTrue:
        description += "PolicyTrue";
        break;
      case Policy::kFalse:
        description += "PolicyFalse";
        break;
    }
    description += "_";
    switch (std::get<1>(info.param)) {
      case FeatureState::kEnabled:
        description += "FeatureEnabled";
        break;
      case FeatureState::kDisabled:
        description += "FeatureDisabled";
        break;
    }
    description += "_";
    if (std::get<2>(info.param)) {
      description += "OriginTrialPresent";
    } else {
      description += "OriginTrialAbsent";
    }
    return description;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FeatureState feature_state = std::get<1>(GetParam());
    if (feature_state == FeatureState::kEnabled) {
      feature_list_.InitWithFeatures({blink::features::kXSLT},
                                     {blink::features::kXSLTSpecialTrial});
    } else {
      feature_list_.InitWithFeatures({}, {blink::features::kXSLT});
    }
    // The test public key, see:
    // https://chromium.googlesource.com/chromium/src/+/main/docs/origin_trials_integration.md
    command_line->AppendSwitchASCII(
        "origin-trial-public-key",
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=");
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    Policy policy_val = std::get<0>(GetParam());
    if (policy_val == Policy::kDefault) {
      return;
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kXSLTEnabled,
              base::Value(policy_val == Policy::kTrue));
    UpdateProviderPolicy(policies);
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            &XSLTPolicyBrowserTest::InterceptRequest, base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    PolicyTest::TearDownOnMainThread();
  }

  bool InterceptRequest(content::URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url.host() != "example.com") {
      return false;
    }

    const bool inject_OT_token = std::get<2>(GetParam());

    std::string headers = "HTTP/1.1 200 OK\n";
    base::StrAppend(&headers, {"Content-Type: text/html; charset=utf-8\n"});
    if (inject_OT_token) {
      base::StrAppend(
          &headers,
          {"Origin-Trial: "
           "A1+hez7wjW7oxcSp/ned30T2klwP/J3eaV/kHc3iZBoKICKOSHAHP7u2h+lgeIH/"
           "MvXfAbLGwUOL9++"
           "lEfOUdQsAAABOeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZm"
           "VhdHVyZSI6ICJYU0xUIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9\n"});
    }
    headers += '\n';

    std::string body =
        "<!DOCTYPE "
        "html><html><head><meta charset=\"utf-8\"></head><body></body></html>";
    content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                 params->client.get());
    return true;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

IN_PROC_BROWSER_TEST_P(XSLTPolicyBrowserTest, PolicyIsFollowed) {
  Policy policy_val = std::get<0>(GetParam());
  FeatureState feature_state = std::get<1>(GetParam());
  bool origin_trial_present = std::get<2>(GetParam());

  bool expected_enabled = false;
  if (policy_val == Policy::kTrue) {
    expected_enabled = true;
  } else if (policy_val == Policy::kFalse) {
    expected_enabled = false;
  } else {
    // Default
    expected_enabled =
        (feature_state == FeatureState::kEnabled) || origin_trial_present;
  }

  const GURL url("https://example.com/xslt.html");
  ASSERT_TRUE(NavigateToUrl(url, this));

  content::DOMMessageQueue message_queue(
      chrome_test_utils::GetActiveWebContents(this));
  content::ExecuteScriptAsync(chrome_test_utils::GetActiveWebContents(this),
                              R"(
        try {
          new XSLTProcessor();
          // XSLT Enabled:
          window.domAutomationController.send(true);
        } catch {
          // XSLT Disabled:
          window.domAutomationController.send(false);
        }
      )");
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_TRUE(message == "true" || message == "false");
  EXPECT_EQ(message == "true", expected_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    XSLTPolicyBrowserTest,
    ::testing::Combine(
        ::testing::Values(Policy::kDefault, Policy::kTrue, Policy::kFalse),
        ::testing::Values(FeatureState::kEnabled, FeatureState::kDisabled),
        ::testing::Bool()),
    &XSLTPolicyBrowserTest::DescribeParams);

}  // namespace policy
