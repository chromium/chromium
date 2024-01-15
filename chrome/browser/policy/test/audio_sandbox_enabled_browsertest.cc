// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/media/audio_service_util.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "sandbox/policy/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class AudioSandboxEnabledTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          /*policy::key::kAllowAudioSandbox=*/std::optional<bool>> {
 public:
  // InProcessBrowserTest implementation:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::PolicyMap values;
    if (GetParam().has_value()) {
      values.Set(policy::key::kAudioSandboxEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(*GetParam()),
                 nullptr);
    }
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUp();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(AudioSandboxEnabledTest, IsRespected) {
  std::optional<bool> enable_sandbox_via_policy = GetParam();
  bool is_sandbox_enabled_by_default =
      base::FeatureList::IsEnabled(features::kAudioServiceSandbox);

  ASSERT_EQ(enable_sandbox_via_policy.value_or(is_sandbox_enabled_by_default),
            IsAudioServiceSandboxEnabled());
}

INSTANTIATE_TEST_SUITE_P(
    Enabled,
    AudioSandboxEnabledTest,
    ::testing::Values(/*policy::key::kAudioSandboxEnabled=*/true));

INSTANTIATE_TEST_SUITE_P(
    Disabled,
    AudioSandboxEnabledTest,
    ::testing::Values(/*policy::key::kAudioSandboxEnabled=*/false));

INSTANTIATE_TEST_SUITE_P(
    NotSet,
    AudioSandboxEnabledTest,
    ::testing::Values(/*policy::key::kAudioSandboxEnabled=*/std::nullopt));

}  // namespace policy
