// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_handler.h"

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/glic/host/glic.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

struct GlicInvokeHandlerTestCase {
  std::string test_name;
  bool expected_result = false;
  bool has_auto_submit_passkey = false;
  std::optional<std::vector<std::string>> prompts;
  mojom::FeatureMode feature_mode = mojom::FeatureMode::kUnspecified;
  bool disable_zero_state_suggestions = false;
  mojom::ActuationTarget actuation_target = mojom::ActuationTarget::kUnknown;
};

class GlicInvokeHandlerTest
    : public testing::TestWithParam<GlicInvokeHandlerTestCase> {};

TEST_P(GlicInvokeHandlerTest, RequiresClientInvoke) {
  const auto& param = GetParam();
  auto mojo_options = mojom::InvokeOptions::New();

  if (param.prompts.has_value()) {
    mojo_options->prompts = param.prompts.value();
  }
  mojo_options->feature_mode = param.feature_mode;
  mojo_options->disable_zero_state_suggestions =
      param.disable_zero_state_suggestions;
  mojo_options->actuation_target = param.actuation_target;

  EXPECT_EQ(param.expected_result,
            GlicInvokeHandler::RequiresClientInvoke(
                mojo_options, param.has_auto_submit_passkey));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicInvokeHandlerTest,
    testing::Values(
        GlicInvokeHandlerTestCase{.test_name = "Empty",
                                  .expected_result = false},
        GlicInvokeHandlerTestCase{.test_name = "WithPasskey",
                                  .expected_result = true,
                                  .has_auto_submit_passkey = true},
        GlicInvokeHandlerTestCase{.test_name = "WithPrompts",
                                  .expected_result = true,
                                  .prompts = std::vector<std::string>{"Hello"}},
        GlicInvokeHandlerTestCase{.test_name = "EmptyPrompts",
                                  .expected_result = false,
                                  .prompts = std::vector<std::string>()},
        GlicInvokeHandlerTestCase{
            .test_name = "WithFeatureMode",
            .expected_result = true,
            .feature_mode = mojom::FeatureMode::kActuation},
        GlicInvokeHandlerTestCase{.test_name = "WithDisableZss",
                                  .expected_result = true,
                                  .disable_zero_state_suggestions = true},
        GlicInvokeHandlerTestCase{
            .test_name = "WithActuationTarget",
            .expected_result = true,
            .actuation_target = mojom::ActuationTarget::kTargetSurface}),
    [](const testing::TestParamInfo<GlicInvokeHandlerTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace glic
