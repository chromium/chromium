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
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace supervised_user {
namespace {

// Whether the ContentSettingsProvider for Cookies is supervised.
enum class CookiesContentSettingsProviderSupervision {
  kSupervised,
  kNonSupervised
};

using CookiesContentSettingsProviderObserver =
    ui::test::PollingStateObserver<CookiesContentSettingsProviderSupervision>;

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(CookiesContentSettingsProviderObserver,
                                    kCookiesContentSettingIsSupervised);

// UI test for the "Cookies" switch from Family Link parental controls.
class SupervisedUserFamilyLinkCookiesSwitchUiTest
    : public InteractiveBrowserTestT<FamilyLiveTest>,
      public testing::WithParamInterface<
          std::tuple<FamilyIdentifier,
                     /*cookies_switch_value=*/bool>> {
 public:
  SupervisedUserFamilyLinkCookiesSwitchUiTest()
      : InteractiveBrowserTestT<FamilyLiveTest>(
            /*family_identifier=*/std::get<0>(GetParam()),
            /*extra_enabled_hosts=*/std::vector<std::string>()) {}

 protected:
  // Parent navigates to FL control page and waits for it to load.
  auto ParentOpensControlPage(ui::ElementIdentifier kParentTab,
                              const GURL& gurl) {
    return Steps(NavigateWebContents(kParentTab, gurl),
                 WaitForWebContentsReady(kParentTab, gurl));
  }

  // Parent toggles the "Cookies" switch in FL, if it does not have
  // the desired value.
  auto ParentSetsCookiesSwitch(ui::ElementIdentifier kParentTab,
                               bool switch_target_value) {
    return Steps(
        ExecuteJs(kParentTab,
                  base::StringPrintf(R"js(
          () => {
            const button = document.querySelector('[aria-label="Toggle permissions for cookies"]');
            if (!button) {
              throw Error("'Cookies' toggle not found.");
            }
            if (button.ariaChecked != "%s") {
              button.click();
            }
          }
        )js",
                                     switch_target_value ? "true" : "false")));
  }

  auto PollCookiesContentSettingProvider(
      ui::test::StateIdentifier<CookiesContentSettingsProviderObserver>
          content_settings_provider_observer) {
    return Steps(PollState(
        content_settings_provider_observer,
        // Checks if Cookies content setting is supervised.
        [this]() -> CookiesContentSettingsProviderSupervision {
          content_settings::ProviderType provider_type;
          HostContentSettingsMap* map =
              HostContentSettingsMapFactory::GetForProfile(
                  child().browser()->profile());

          map->GetDefaultContentSetting(ContentSettingsType::COOKIES,
                                        &provider_type);
          return provider_type ==
                         content_settings::ProviderType::kSupervisedProvider
                     ? CookiesContentSettingsProviderSupervision::kSupervised
                     : CookiesContentSettingsProviderSupervision::
                           kNonSupervised;
        }));
  }
};

// Tests that Chrome receives the value of the "Cookies" switch from
// Family Link parental controls.
IN_PROC_BROWSER_TEST_P(SupervisedUserFamilyLinkCookiesSwitchUiTest,
                       CookiesSwitchToggleReceivedByChromeTest) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kParentControlsTab);
  int parent_tab_index = 0;
  bool cookies_switch_target_value = std::get<1>(GetParam());
  // When the "Cookies" switch is OFF, the cookies content settings are
  // supervised, when it it ON, they are not superived.
  CookiesContentSettingsProviderSupervision
      target_cookies_content_settings_supervision =
          cookies_switch_target_value
              ? CookiesContentSettingsProviderSupervision::kNonSupervised
              : CookiesContentSettingsProviderSupervision::kSupervised;

  CookiesContentSettingsProviderSupervision
      precond_cookies_content_settings_supervision =
          cookies_switch_target_value
              ? CookiesContentSettingsProviderSupervision::kSupervised
              : CookiesContentSettingsProviderSupervision::kNonSupervised;

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  RunTestSequence(Steps(
      InstrumentTab(kParentControlsTab, parent_tab_index,
                    head_of_household().browser()),

      ParentOpensControlPage(kParentControlsTab,
                             head_of_household().GetPermissionsUrlFor(child())),
      // Observe if the content settings provider for cookies is supervised.
      PollCookiesContentSettingProvider(kCookiesContentSettingIsSupervised),

      // Precondition: Set the switch to opposite from the target value, if it's
      // not already.
      // TODO(b/303401498): use dedicated RPCs one available.
      ParentSetsCookiesSwitch(kParentControlsTab, !cookies_switch_target_value),
      WaitForState(kCookiesContentSettingIsSupervised,
                   precond_cookies_content_settings_supervision),

      // Toggle the switch and confirm the content settings provider for
      // cookies is updated.
      ParentSetsCookiesSwitch(kParentControlsTab, cookies_switch_target_value),
      WaitForState(kCookiesContentSettingIsSupervised,
                   target_cookies_content_settings_supervision)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserFamilyLinkCookiesSwitchUiTest,
    testing::Combine(
        testing::Values(FamilyIdentifier("FAMILY_DMA_ELIGIBLE_WITH_CONSENT"),
                        FamilyIdentifier("FAMILY_DMA_ELIGIBLE_NO_CONSENT"),
                        FamilyIdentifier("FAMILY_DMA_INELIGIBLE")),
        /*cookies_switch_value=*/testing::Bool()),
    [](const auto& info) {
      return std::string(std::get<0>(info.param)->data()) +
             std::string((std::get<1>(info.param) ? "WithCookiesSwitchOn"
                                                  : "WithCookiesSwitchOff"));
    });
}  // namespace
}  // namespace supervised_user
