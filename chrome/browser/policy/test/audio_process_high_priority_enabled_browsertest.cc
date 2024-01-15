// Copyright 2021 The Chromium Authors
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

class AudioProcessHighPriorityEnabledTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          /*policy::key::kAudioProcessHighPriorityEnabled=*/std::optional<
              bool>> {
 public:
  // InProcessBrowserTest implementation:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::PolicyMap values;
    if (GetParam().has_value()) {
      values.Set(policy::key::kAudioProcessHighPriorityEnabled,
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

IN_PROC_BROWSER_TEST_P(AudioProcessHighPriorityEnabledTest, IsRespected) {
  std::optional<bool> enable_high_priority_via_policy = GetParam();

  ASSERT_EQ(enable_high_priority_via_policy.value_or(false),
            IsAudioProcessHighPriorityEnabled());
}

INSTANTIATE_TEST_SUITE_P(
    Enabled,
    AudioProcessHighPriorityEnabledTest,
    ::testing::Values(/*policy::key::kAudioProcessHighPriorityEnabled=*/true));

INSTANTIATE_TEST_SUITE_P(
    Disabled,
    AudioProcessHighPriorityEnabledTest,
    ::testing::Values(/*policy::key::kAudioProcessHighPriorityEnabled=*/false));

INSTANTIATE_TEST_SUITE_P(
    NotSet,
    AudioProcessHighPriorityEnabledTest,
    ::testing::Values(
        /*policy::key::kAudioProcessHighPriorityEnabled=*/std::nullopt));

}  // namespace policy
