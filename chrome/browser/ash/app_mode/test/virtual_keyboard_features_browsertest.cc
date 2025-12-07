// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::AutoLaunchKioskApp;
using kiosk::test::CachePolicy;
using kiosk::test::WaitKioskLaunched;

namespace {

bool IsChromeApp(const KioskApp& app) {
  return app.id().type == KioskAppType::kChromeApp;
}

std::string ToJsonString(const keyboard::KeyboardConfig& config) {
  auto dict = base::Value::Dict()
                  .Set("auto_complete_enabled", config.auto_complete)
                  .Set("auto_correct_enabled", config.auto_correct)
                  .Set("handwriting_enabled", config.handwriting)
                  .Set("spell_check_enabled", config.spell_check)
                  .Set("voice_input_enabled", config.voice_input);
  std::string json_string;
  CHECK(base::JSONWriter::Write(dict, &json_string));
  return json_string;
}

// Returns the `KeyboardConfig` used by default when the policy is unset.
keyboard::KeyboardConfig UnsetKeyboardConfig() {
  // Note that `KeyboardConfig.auto_capitalize` is not configurable by policy.
  // Other fields default to `false` when the policy is unset.
  return keyboard::KeyboardConfig{
      .auto_complete = false,
      .auto_correct = false,
      .handwriting = false,
      .spell_check = false,
      .voice_input = false,
  };
}

// The parameter used to define a name and a `KeyboardConfig` for the test.
using TestParam =
    std::tuple<std::string, std::optional<keyboard::KeyboardConfig>>;

// Returns a name like "ChromeAppWithMixedValuesPolicy" for the test parameter.
std::string ParamName(
    const testing::TestParamInfo<std::tuple<KioskMixin::Config, TestParam>>&
        info) {
  auto [kiosk_mixin_config, test_param] = info.param;
  auto [test_param_name, _] = test_param;
  auto index = base::StringPrintf("App%zu", info.index);
  return kiosk_mixin_config.name.value_or(index) + test_param_name;
}

// Caches the given `keyboard_config` in fake session manager.
void CacheKeyboardPolicy(
    const std::string& account_id,
    const std::optional<keyboard::KeyboardConfig>& keyboard_config) {
  if (!keyboard_config.has_value()) {
    return;
  }

  CachePolicy(account_id,
              [keyboard_config](policy::UserPolicyBuilder& builder) {
                builder.payload().mutable_virtualkeyboardfeatures()->set_value(
                    ToJsonString(keyboard_config.value()));
              });
}

}  // namespace

// Verifies the `VirtualKeyboardFeatures` applies correctly in Kiosk.
class VirtualKeyboardFeaturesTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<
          std::tuple<KioskMixin::Config, TestParam>> {
 public:
  VirtualKeyboardFeaturesTest() = default;
  VirtualKeyboardFeaturesTest(const VirtualKeyboardFeaturesTest&) = delete;
  VirtualKeyboardFeaturesTest& operator=(const VirtualKeyboardFeaturesTest&) =
      delete;

  ~VirtualKeyboardFeaturesTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    const auto& auto_launch_account_id =
        config().auto_launch_account_id.value();
    CacheKeyboardPolicy(auto_launch_account_id.value(),
                        policy_keyboard_config());
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  const KioskMixin::Config& config() {
    auto& [config, _] = GetParam();
    return config;
  }

  const std::optional<keyboard::KeyboardConfig>& policy_keyboard_config() {
    auto& [_, keyboard_config] = std::get<TestParam>(GetParam());
    return keyboard_config;
  }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/config()};
};

IN_PROC_BROWSER_TEST_P(VirtualKeyboardFeaturesTest,
                       KeyboardConfigRespectsPolicy) {
  const auto config = KeyboardController::Get()->GetKeyboardConfig();

  // `auto_capitalize` is not controlled by policy and remain the default.
  EXPECT_EQ(config.auto_capitalize, keyboard::KeyboardConfig().auto_capitalize);

  // Chrome apps are not affected by this policy and remain the default. Web
  // apps get the policy value if set, or the unset config otherwise.
  auto expected_config =
      IsChromeApp(AutoLaunchKioskApp())
          ? keyboard::KeyboardConfig()
          : policy_keyboard_config().value_or(UnsetKeyboardConfig());

  EXPECT_EQ(config.auto_complete, expected_config.auto_complete);
  EXPECT_EQ(config.auto_correct, expected_config.auto_correct);
  EXPECT_EQ(config.handwriting, expected_config.handwriting);
  EXPECT_EQ(config.spell_check, expected_config.spell_check);
  EXPECT_EQ(config.voice_input, expected_config.voice_input);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    VirtualKeyboardFeaturesTest,
    testing::Combine(
        testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
        testing::Values(TestParam{"WithoutPolicy", {}},
                        TestParam{"WithAllFalsePolicy",
                                  keyboard::KeyboardConfig{
                                      .auto_complete = false,
                                      .auto_correct = false,
                                      .handwriting = false,
                                      .spell_check = false,
                                      .voice_input = false,
                                  }},
                        TestParam{"WithAllTruePolicy",
                                  keyboard::KeyboardConfig{
                                      .auto_complete = true,
                                      .auto_correct = true,
                                      .handwriting = true,
                                      .spell_check = true,
                                      .voice_input = true,
                                  }},
                        TestParam{"WithMixedValuesPolicy",
                                  keyboard::KeyboardConfig{
                                      .auto_complete = true,
                                      .auto_correct = false,
                                      .handwriting = true,
                                      .spell_check = false,
                                      .voice_input = true,
                                  }})),
    ParamName);

}  // namespace ash
