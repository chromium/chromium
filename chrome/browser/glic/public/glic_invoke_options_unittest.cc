// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_invoke_options.h"

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/glic/test_support/glic_api_test_passkeys.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicInvokeOptionsTest, EveryInvokeErrorHasADistinctString) {
  std::set<std::string_view> seen;
  // 0 is reserved for success in metrics, so values start at 1.
  for (int value = 1; value <= static_cast<int>(GlicInvokeError::kMaxValue);
       ++value) {
    // Skip the deprecated kInstanceDestroyed.
    if (value == 6) {
      continue;
    }
    const auto error = static_cast<GlicInvokeError>(value);
    const std::string_view description = GlicInvokeErrorToString(error);
    EXPECT_FALSE(description.empty()) << "No description for error " << value;
    EXPECT_TRUE(seen.insert(description).second)
        << "Duplicate description \"" << description << "\" for error "
        << value;
  }
}

TEST(GlicInvokeOptionsTest, RequiresContextClipboardPolicyCheck) {
  GlicInvokeOptions options(glic::mojom::InvocationSource::kOsButton);
  EXPECT_FALSE(options.RequiresContextClipboardPolicyCheck());

  options.additional_context.emplace(glic::mojom::AdditionalContext::New(),
                                     content::GlobalRenderFrameHostId(1, 2),
                                     PolicyCheck::kClipboard);
  EXPECT_TRUE(options.RequiresContextClipboardPolicyCheck());

  options.additional_context->policy_check = PolicyCheck::kNone;
  EXPECT_FALSE(options.RequiresContextClipboardPolicyCheck());
}

TEST(GlicInvokeOptionsTest, RequiresPromptClipboardPolicyCheck) {
  GlicInvokeOptions options(glic::mojom::InvocationSource::kOsButton);
  EXPECT_FALSE(options.RequiresPromptClipboardPolicyCheck());

  options.prompts = {"hello"};
  options.prompt_policy = PromptPolicy{content::GlobalRenderFrameHostId(1, 2),
                                       PolicyCheck::kClipboard};
  EXPECT_TRUE(options.RequiresPromptClipboardPolicyCheck());

  options.prompt_policy->policy_check = PolicyCheck::kNone;
  EXPECT_FALSE(options.RequiresPromptClipboardPolicyCheck());

  options.prompts.clear();
  EXPECT_CHECK_DEATH(options.RequiresPromptClipboardPolicyCheck(),
                     base::NotFatalUntil::M163);
}

TEST(GlicInvokeOptionsTest, OnErrorWithDiagnosticsCallbackMoveSemantics) {
  std::optional<std::pair<GlicInvokeError, std::string>> received;
  GlicInvokeOptions options(glic::mojom::InvocationSource::kOsButton);
  options.SetOnErrorWithDiagnostics(
      GlicApiTestPasskeys::GetErrorDiagnosticsPassKey(),
      base::BindOnce(
          [](std::optional<std::pair<GlicInvokeError, std::string>>* out,
             GlicInvokeError error, std::string diagnostic_info) {
            *out = std::make_pair(error, std::move(diagnostic_info));
          },
          &received));

  GlicInvokeOptions moved_options(std::move(options));
  auto callback = moved_options.TakeOnErrorWithDiagnostics(
      GlicApiTestPasskeys::GetErrorDiagnosticsPassKey());
  ASSERT_TRUE(callback);
  std::move(callback).Run(
      GlicInvokeError::kClientLoadError,
      "stage=WAIT_FOR_CLIENT_READY;load_state_at_start=LOADING;load_error="
      "OFFLINE");
  EXPECT_EQ(received,
            std::make_pair(
                GlicInvokeError::kClientLoadError,
                std::string("stage=WAIT_FOR_CLIENT_READY;load_state_at_start="
                            "LOADING;load_error=OFFLINE")));
}

}  // namespace
}  // namespace glic
