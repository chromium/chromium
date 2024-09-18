// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <tuple>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/family_live_test.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/browser_state_management.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace supervised_user {
namespace {

FamilyLiveTest::RpcMode GetRpcMode(auto test_param) {
  return std::get<0>(test_param);
}

FamilyLinkToggleType GetSwitchType(auto test_param) {
  return std::get<1>(test_param);
}

FamilyLinkToggleState GetSwitchTargetState(auto test_param) {
  return std::get<2>(test_param);
}

// Live test for the Family Link Advanced Settings parental controls switches.
// TODO(b/301587955): Fix placement of supervised_user/e2e test files and their
// dependencies.
class SupervisedUserFamilyLinkSwitchTest
    : public InteractiveFamilyLiveTest,
      public testing::WithParamInterface<std::tuple<FamilyLiveTest::RpcMode,
                                                    FamilyLinkToggleType,
                                                    FamilyLinkToggleState>> {
 public:
  SupervisedUserFamilyLinkSwitchTest()
      : InteractiveFamilyLiveTest(GetRpcMode(GetParam())) {}
};

// Tests that Chrome receives the value of the given switch from
// Family Link parental controls.
IN_PROC_BROWSER_TEST_P(SupervisedUserFamilyLinkSwitchTest,
                       SwitchToggleReceivedByChromeTest) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kDefineStateObserverId);
  TurnOnSync();

  // Set the cookies switch on FL confirm the setting is received by Chrome.
  RunTestSequence(WaitForStateSeeding(
      kDefineStateObserverId, child(),
      BrowserState::AdvancedSettingsToggles({FamilyLinkToggleConfiguration(
          {.type = GetSwitchType(GetParam()),
           .state = GetSwitchTargetState(GetParam())})})));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserFamilyLinkSwitchTest,
    testing::Combine(
        testing::Values(FamilyLiveTest::RpcMode::kProd,
                        FamilyLiveTest::RpcMode::kTestImpersonation),
        testing::Values(FamilyLinkToggleType::kPermissionsToggle,
                        FamilyLinkToggleType::kCookiesToggle),
        testing::Values(FamilyLinkToggleState::kEnabled,
                        FamilyLinkToggleState::kDisabled)),
    [](const auto& info) {
      return ToString(GetRpcMode(info.param)) +
             std::string((GetSwitchType(info.param) ==
                                  FamilyLinkToggleType::kCookiesToggle
                              ? "_ForCookiesSwitch"
                              : "_ForPermissionsSwitch")) +
             std::string((GetSwitchTargetState(info.param) ==
                                  FamilyLinkToggleState::kEnabled
                              ? "_WithSwitchOn"
                              : "_WithSwitchOff"));
    });
}  // namespace
}  // namespace supervised_user
