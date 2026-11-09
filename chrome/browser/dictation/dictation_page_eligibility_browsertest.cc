// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/dictation_context.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace dictation {

namespace {

constexpr char kTestHost[] = "a.test";
constexpr char kTestPagePath[] = "/simple.html";

}  // namespace

// End-to-end tests for gating page context extraction on the Optimization
// Guide DICTATION_ELIGIBLE hint. Note that this is not related to the
// CSE/IRM-related `optimization_guide::PageContextEligibility` checks also
// done in Dictation.
class DictationPageEligibilityBrowserTest : public DictationBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    DictationBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  GURL GetTestPageUrl() {
    return embedded_test_server()->GetURL(kTestHost, kTestPagePath);
  }

  // Navigates to the test page, starts dictation and returns the context sent
  // with the stream start.
  std::optional<DictationContext> StartDictationOnTestPage() {
    if (!content::NavigateToURL(web_contents(), GetTestPageUrl())) {
      ADD_FAILURE() << "Failed to navigate to " << GetTestPageUrl();
      return std::nullopt;
    }
    StartSession();
    ListenerStreamProvider* provider = attached_stream();
    if (!provider) {
      ADD_FAILURE() << "No stream provider";
      return std::nullopt;
    }
    ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
    return ExtensionGetStartStreamDetails(profile(),
                                          provider->stream_id_for_testing());
  }
};

IN_PROC_BROWSER_TEST_F(DictationPageEligibilityBrowserTest,
                       EligiblePageContextCaptured) {
  AddOptimizationGuideEligibleHint(GetTestPageUrl());

  std::optional<DictationContext> context = StartDictationOnTestPage();
  ASSERT_TRUE(context.has_value());

  EXPECT_TRUE(context->annotated_page_content.has_value());
  EXPECT_TRUE(context->inner_text.has_value());
}

IN_PROC_BROWSER_TEST_F(DictationPageEligibilityBrowserTest,
                       PageWithoutHintContextElided) {
  std::optional<DictationContext> context = StartDictationOnTestPage();
  ASSERT_TRUE(context.has_value());

  EXPECT_FALSE(context->annotated_page_content.has_value());
  EXPECT_FALSE(context->inner_text.has_value());
  EXPECT_FALSE(context->editable_content.has_value());
}

// The server also sends a hint for pages that fail its safety checks, with the
// verdict in the metadata.
IN_PROC_BROWSER_TEST_F(DictationPageEligibilityBrowserTest,
                       IneligiblePageContextElided) {
  AddOptimizationGuideIneligibleHint(GetTestPageUrl());

  std::optional<DictationContext> context = StartDictationOnTestPage();
  ASSERT_TRUE(context.has_value());

  EXPECT_FALSE(context->annotated_page_content.has_value());
  EXPECT_FALSE(context->inner_text.has_value());
  EXPECT_FALSE(context->editable_content.has_value());
}

}  // namespace dictation
