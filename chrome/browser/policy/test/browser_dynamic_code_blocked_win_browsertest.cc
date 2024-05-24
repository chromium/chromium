// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

struct TestCase {
  std::string name;
  std::optional<int> policy_value;
  bool should_be_blocked;
};

// Placeholder func that must not be optimized out by the compiler.
void Func() {
  volatile int a = 0;
  std::ignore = a;
}

}  // namespace

class BrowserDynamicCodeBlockedTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<TestCase> {
 public:
  // InProcessBrowserTest implementation:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::PolicyMap values;
    if (GetParam().policy_value.has_value()) {
      values.Set(policy::key::kDynamicCodeSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(GetParam().policy_value.value()), nullptr);
    }
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(BrowserDynamicCodeBlockedTest, IsRespected) {
  // This mitigation only does anything on Win10 RS1 and above.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1) {
    GTEST_SKIP();
  }

  DWORD old_protect;
  // VirtualProtect will fail to mark memory RWX if dynamic code is blocked in
  // the browser process.
  ASSERT_EQ(GetParam().should_be_blocked,
            !::VirtualProtect(reinterpret_cast<uintptr_t*>(&Func),
                              /*dwSize=*/sizeof(uintptr_t),
                              PAGE_EXECUTE_READWRITE, &old_protect));
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    BrowserDynamicCodeBlockedTest,
    testing::ValuesIn<TestCase>({{.name = "Default",
                                  .policy_value = /*Default=*/0,
                                  .should_be_blocked = false},
                                 {.name = "EnabledForBrowser",
                                  .policy_value = /*EnabledForBrowser=*/1,
                                  .should_be_blocked = true},
                                 {.name = "NotSet",
                                  .policy_value = std::nullopt,
                                  .should_be_blocked = false}}),
    [](const auto& info) { return info.param.name; });

}  // namespace policy
