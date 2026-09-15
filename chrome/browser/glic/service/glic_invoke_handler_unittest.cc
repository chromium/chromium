// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "content/public/browser/global_routing_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

struct GlicInvokeHandlerTestCase {
  std::string test_name;
  bool expected_result = false;
  bool has_auto_submit_passkey = false;
  mojom::InvocationSource invocation_source =
      mojom::InvocationSource::kOsButton;
  std::vector<std::string> prompts;
  std::optional<mojom::FeatureMode> feature_mode;
  bool disable_zss = false;
  mojom::ActuationTarget actuation_target =
      mojom::ActuationTarget::kAgentDecides;
  std::optional<std::string> skill_id;
  std::optional<ZssConfig> zss_config;
  bool has_additional_context = false;
  bool has_empty_additional_context = false;
};

class GlicInvokeHandlerTest
    : public testing::TestWithParam<GlicInvokeHandlerTestCase> {};

TEST_P(GlicInvokeHandlerTest, RequiresClientInvoke) {
  const auto& param = GetParam();

  GlicInvokeOptions options(param.invocation_source);
  options.prompts = param.prompts;
  options.feature_mode = param.feature_mode;
  options.disable_zss = param.disable_zss;
  options.target.actuation_target = param.actuation_target;
  options.skill_id = param.skill_id;
  options.zss_config = param.zss_config;
  if (param.has_additional_context || param.has_empty_additional_context) {
    mojom::AdditionalContextPtr context;
    if (param.has_additional_context) {
      context = mojom::AdditionalContext::New();
    }
    options.additional_context.emplace(std::move(context),
                                       content::GlobalRenderFrameHostId(),
                                       PolicyCheck::kNone);
  }

  EXPECT_EQ(param.expected_result, GlicInvokeHandler::RequiresClientInvoke(
                                       options, param.has_auto_submit_passkey));
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
        GlicInvokeHandlerTestCase{
            .test_name = "WithCaptureRegionHotkey",
            .expected_result = true,
            .invocation_source = mojom::InvocationSource::kCaptureRegionHotkey},
        GlicInvokeHandlerTestCase{.test_name = "WithPrompts",
                                  .expected_result = true,
                                  .prompts = {"Hello"}},
        GlicInvokeHandlerTestCase{.test_name = "EmptyPrompts",
                                  .expected_result = false,
                                  .prompts = {}},
        GlicInvokeHandlerTestCase{
            .test_name = "WithFeatureMode",
            .expected_result = true,
            .feature_mode = mojom::FeatureMode::kActuation},
        GlicInvokeHandlerTestCase{
            .test_name = "WithUnspecifiedFeatureMode",
            .expected_result = false,
            .feature_mode = mojom::FeatureMode::kUnspecified},
        GlicInvokeHandlerTestCase{.test_name = "WithDisableZss",
                                  .expected_result = true,
                                  .disable_zss = true},
        GlicInvokeHandlerTestCase{
            .test_name = "WithActuationTarget",
            .expected_result = true,
            .actuation_target = mojom::ActuationTarget::kTargetSurface},
        GlicInvokeHandlerTestCase{
            .test_name = "WithAgentDecidesActuationTarget",
            .expected_result = false,
            .actuation_target = mojom::ActuationTarget::kAgentDecides},
        GlicInvokeHandlerTestCase{
            .test_name = "WithUnknownActuationTarget",
            .expected_result = false,
            .actuation_target = mojom::ActuationTarget::kUnknown},
        GlicInvokeHandlerTestCase{.test_name = "WithSkillId",
                                  .expected_result = true,
                                  .skill_id = "skill"},
        GlicInvokeHandlerTestCase{.test_name = "WithZssConfig",
                                  .expected_result = true,
                                  .zss_config = ZssConfig()},
        GlicInvokeHandlerTestCase{.test_name = "WithAdditionalContext",
                                  .expected_result = true,
                                  .has_additional_context = true},
        GlicInvokeHandlerTestCase{.test_name = "WithEmptyAdditionalContext",
                                  .expected_result = false,
                                  .has_empty_additional_context = true}),
    [](const testing::TestParamInfo<GlicInvokeHandlerTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace glic
