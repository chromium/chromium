// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/new_tab_page_util.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/test/test_sync_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

const char kSampleUserEmail[] = "user@gmail.com";

base::Value::List CreatePolicyList(const std::string& name,
                                   const std::string& url) {
  base::Value::Dict shortcut_item;
  shortcut_item.Set("name", name);
  shortcut_item.Set("url", url);
  base::Value::List policy_list;
  policy_list.Append(std::move(shortcut_item));
  return policy_list;
}

}  // namespace

class NewTabPageUtilBrowserTest : public SigninBrowserTestBase,
                                  public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    SigninBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    if (GetParam()) {
      SetAccountsCookiesAndTokens({kSampleUserEmail});
    }
  }

  void SetSync(bool sync_enabled) {
    GetTestSyncService()->SetSignedIn(sync_enabled
                                          ? signin::ConsentLevel::kSync
                                          : signin::ConsentLevel::kSignin);
  }

  void SetHistorySync(bool sync_enabled) {
    GetTestSyncService()->SetSignedIn(signin::ConsentLevel::kSignin);
    GetTestSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, sync_enabled);
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Disable the field trial testing config as the tests in this file care
    // about whether features are overridden or not.
    cmd->AppendSwitch(variations::switches::kDisableFieldTrialTestingConfig);
    cmd->AppendSwitch(optimization_guide::switches::kDebugLoggingEnabled);
  }

  OptimizationGuideKeyedService* GetOptimizationGuideKeyedService() {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile());
  }

  void CheckInternalsLog(std::string_view message) {
    auto* logger =
        GetOptimizationGuideKeyedService()->GetOptimizationGuideLogger();
    EXPECT_THAT(logger->recent_log_messages_,
                testing::Contains(testing::Field(
                    &OptimizationGuideLogger::LogMessage::message,
                    testing::HasSubstr(message))));
  }

  base::test::ScopedFeatureList& features() { return features_; }
  policy::MockConfigurationPolicyProvider& policy_provider() {
    return policy_provider_;
  }

 private:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBase::OnWillCreateBrowserContextServices(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
  }

  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }

  base::test::ScopedFeatureList features_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

class NewTabPageUtilEnableFlagBrowserTest : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilEnableFlagBrowserTest() {
    features().InitWithFeatures(
        {ntp_features::kNtpChromeCartModule, ntp_features::kNtpDriveModule,
         ntp_features::kNtpCalendarModule,
         ntp_features::kNtpMicrosoftAuthenticationModule,
         ntp_features::kNtpOutlookCalendarModule,
         ntp_features::kNtpSharepointModule},
        {});
  }
};

class NewTabPageUtilDisableFlagBrowserTest : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilDisableFlagBrowserTest() {
    features().InitWithFeatures(
        {}, {ntp_features::kNtpChromeCartModule, ntp_features::kNtpDriveModule,
             ntp_features::kNtpCalendarModule,
             ntp_features::kNtpMicrosoftAuthenticationModule,
             ntp_features::kNtpOutlookCalendarModule,
             ntp_features::kNtpSharepointModule});
  }
};

IN_PROC_BROWSER_TEST_P(NewTabPageUtilBrowserTest, EnableCartByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(IsCartModuleEnabled());
#else
  EXPECT_FALSE(IsCartModuleEnabled());
#endif
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilBrowserTest, DisableCartByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("ca");
  EXPECT_FALSE(IsCartModuleEnabled());
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilEnableFlagBrowserTest, EnableCartByFlag) {
  EXPECT_TRUE(IsCartModuleEnabled());
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilDisableFlagBrowserTest,
                       DisableCartByFlag) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsCartModuleEnabled());
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilBrowserTest, EnableDriveByToT) {
  SetHistorySync(true);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_EQ(
      IsDriveModuleEnabledForProfile(/*is_managed_profile=*/true, GetProfile()),
      GetParam());
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    (GetParam() ? " enabled: default feature flag value"
                                : " disabled: not signed in"));
#else
  EXPECT_FALSE(IsDriveModuleEnabledForProfile(/*is_managed_profile=*/true,
                                              GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    " disabled: default feature flag value");
#endif
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilBrowserTest, Drive_HistorySyncDisabled) {
  SetHistorySync(false);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_FALSE(IsDriveModuleEnabledForProfile(/*is_managed_profile=*/true,
                                              GetProfile()));
  CheckInternalsLog(
      std::string(ntp_features::kNtpDriveModule.name) +
      (GetParam() ? " disabled: no history sync" : " disabled: not signed in"));
#else
  EXPECT_FALSE(IsDriveModuleEnabledForProfile(/*is_managed_profile=*/true,
                                              GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    " disabled: default feature flag value");
#endif
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilEnableFlagBrowserTest, EnableDriveByFlag) {
  EXPECT_EQ(
      IsDriveModuleEnabledForProfile(/*is_managed_profile=*/true, GetProfile()),
      GetParam());
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    (GetParam() ? " enabled: feature flag forced on"
                                : " disabled: not signed in"));
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilDisableFlagBrowserTest,
                       DisableDriveByFlag) {
  EXPECT_FALSE(IsDriveModuleEnabledForProfile(/*is_managed_profile=*/true,
                                              GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    " disabled: feature flag forced off");
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilEnableFlagBrowserTest, DriveIsNotManaged) {
  EXPECT_FALSE(IsDriveModuleEnabledForProfile(/*is_managed_profile=*/false,
                                              GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpDriveModule.name) +
                    (GetParam() ? " disabled: account not managed"
                                : " disabled: not signed in"));
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilEnableFlagBrowserTest,
                       EnableGoogleCalendarByFlag) {
  EXPECT_EQ(
      IsGoogleCalendarModuleEnabled(/*is_managed_profile=*/true, GetProfile()),
      GetParam());
  CheckInternalsLog(std::string(ntp_features::kNtpCalendarModule.name) +
                    (GetParam() ? " enabled: feature flag forced on"
                                : " disabled: not signed in"));
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilDisableFlagBrowserTest,
                       DisableGoogleCalendarByFlag) {
  EXPECT_FALSE(
      IsGoogleCalendarModuleEnabled(/*is_managed_profile=*/true, GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpCalendarModule.name) +
                    (GetParam() ? " disabled: feature flag forced off"
                                : " disabled: not signed in"));
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilEnableFlagBrowserTest,
                       GoogleCalendarIsNotManaged) {
  EXPECT_FALSE(IsGoogleCalendarModuleEnabled(/*is_managed_profile=*/false,
                                             GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpCalendarModule.name) +
                    (GetParam() ? " disabled: account not managed"
                                : " disabled: not signed in"));
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilEnableFlagBrowserTest,
                       EnableMicrosoftFilesByFlag) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kNTPSharepointCardVisible,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_provider().UpdateChromePolicy(policies);
  EXPECT_TRUE(IsMicrosoftFilesModuleEnabledForProfile(GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpSharepointModule.name) +
                    " enabled: feature flag forced on");
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilDisableFlagBrowserTest,
                       DisableMicrosoftFilesByFlag) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kNTPSharepointCardVisible,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_provider().UpdateChromePolicy(policies);
  EXPECT_FALSE(IsMicrosoftFilesModuleEnabledForProfile(GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpSharepointModule.name) +
                    " disabled: feature flag forced off");
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilEnableFlagBrowserTest,
                       MicrosoftFilesPolicyDisabled) {
  EXPECT_FALSE(IsMicrosoftFilesModuleEnabledForProfile(GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpSharepointModule.name) +
                    " disabled: disabled by policy");
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilEnableFlagBrowserTest,
                       EnableOutlookCalendarByFlag) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kNTPOutlookCardVisible,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_provider().UpdateChromePolicy(policies);
  EXPECT_TRUE(IsOutlookCalendarModuleEnabledForProfile(GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpOutlookCalendarModule.name) +
                    " enabled: feature flag forced on");
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilDisableFlagBrowserTest,
                       DisableOutlookCalendarByFlag) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kNTPOutlookCardVisible,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_provider().UpdateChromePolicy(policies);
  EXPECT_FALSE(IsOutlookCalendarModuleEnabledForProfile(GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpOutlookCalendarModule.name) +
                    " disabled: feature flag forced off");
}

IN_PROC_BROWSER_TEST_P(NewTabPageUtilEnableFlagBrowserTest,
                       OutlookCalendarPolicyDisabled) {
  EXPECT_FALSE(IsOutlookCalendarModuleEnabledForProfile(GetProfile()));
  CheckInternalsLog(std::string(ntp_features::kNtpOutlookCalendarModule.name) +
                    " disabled: disabled by policy");
}

class NewTabPageUtilTileTypesEnterpriseShortcutsDisabledBrowserTest
    : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilTileTypesEnterpriseShortcutsDisabledBrowserTest() {
    features().InitWithFeatures({}, {ntp_tiles::kNtpEnterpriseShortcuts});
  }
};

IN_PROC_BROWSER_TEST_P(
    NewTabPageUtilTileTypesEnterpriseShortcutsDisabledBrowserTest,
    GetEnabledTileTypes) {
  // By default, Custom Links should be enabled.
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>({ntp_tiles::TileType::kCustomLinks}));

  // Set enterprise shortcuts policy.
  browser()->profile()->GetPrefs()->SetList(
      ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
      CreatePolicyList("work name", "https://work.com/"));

  // If custom links are explicitly disabled, it falls back to Top Sites.
  browser()->profile()->GetPrefs()->SetBoolean(
      ntp_prefs::kNtpCustomLinksVisible, false);
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>({ntp_tiles::TileType::kTopSites}));

  // Edge case: If enterprise shortcuts are visible (pref=true) but the
  // feature is disabled, code forces Custom Links back on.
  browser()->profile()->GetPrefs()->SetBoolean(
      ntp_prefs::kNtpEnterpriseShortcutsVisible, true);
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>({ntp_tiles::TileType::kCustomLinks}));
}

class NewTabPageUtilTileTypesEnterpriseShortcutsEnabledNoMixingBrowserTest
    : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilTileTypesEnterpriseShortcutsEnabledNoMixingBrowserTest() {
    features().InitWithFeaturesAndParameters(
        {{ntp_tiles::kNtpEnterpriseShortcuts,
          {{ntp_tiles::kNtpEnterpriseShortcutsAllowMixingParam.name,
            "false"}}}},
        {});
  }
};

IN_PROC_BROWSER_TEST_P(
    NewTabPageUtilTileTypesEnterpriseShortcutsEnabledNoMixingBrowserTest,
    GetEnabledTileTypes) {
  // By default, personal shortcuts are visible (Custom Links).
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>({ntp_tiles::TileType::kCustomLinks}));

  // Set enterprise shortcuts policy.
  browser()->profile()->GetPrefs()->SetList(
      ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
      CreatePolicyList("work name", "https://work.com/"));

  // If enterprise shortcuts are enabled and mixing is DISABLED,
  // personal shortcuts (Custom Links) should disappear.
  browser()->profile()->GetPrefs()->SetBoolean(
      ntp_prefs::kNtpEnterpriseShortcutsVisible, true);
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>(
                {ntp_tiles::TileType::kEnterpriseShortcuts}));

  // Remove enterprise shortcuts policy, personal shortcuts should be visible.
  browser()->profile()->GetPrefs()->SetList(
      ntp_tiles::prefs::kEnterpriseShortcutsPolicyList, base::Value::List());
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>({ntp_tiles::TileType::kCustomLinks}));
}

class NewTabPageUtilTileTypesEnterpriseShortcutsEnabledAllowMixingBrowserTest
    : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilTileTypesEnterpriseShortcutsEnabledAllowMixingBrowserTest() {
    features().InitWithFeaturesAndParameters(
        {{ntp_tiles::kNtpEnterpriseShortcuts,
          {{ntp_tiles::kNtpEnterpriseShortcutsAllowMixingParam.name, "true"}}}},
        {});
  }
};

IN_PROC_BROWSER_TEST_P(
    NewTabPageUtilTileTypesEnterpriseShortcutsEnabledAllowMixingBrowserTest,
    GetEnabledTileTypes) {
  // By default, personal shortcuts are visible (Custom Links).
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>({ntp_tiles::TileType::kCustomLinks}));

  // Set enterprise shortcuts policy.
  browser()->profile()->GetPrefs()->SetList(
      ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
      CreatePolicyList("work name", "https://work.com/"));

  // If enterprise shortcuts are also visible AND mixing is ALLOWED,
  // both should be enabled.
  browser()->profile()->GetPrefs()->SetBoolean(
      ntp_prefs::kNtpEnterpriseShortcutsVisible, true);
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>(
                {ntp_tiles::TileType::kCustomLinks,
                 ntp_tiles::TileType::kEnterpriseShortcuts}));

  // If personal shortcuts are explicitly hidden by the user,
  // only enterprise should remain.
  browser()->profile()->GetPrefs()->SetBoolean(
      ntp_prefs::kNtpPersonalShortcutsVisible, false);
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>(
                {ntp_tiles::TileType::kEnterpriseShortcuts}));

  // Remove enterprise shortcuts policy, personal shortcuts should be visible.
  browser()->profile()->GetPrefs()->SetList(
      ntp_tiles::prefs::kEnterpriseShortcutsPolicyList, base::Value::List());
  EXPECT_EQ(GetEnabledTileTypes(browser()->profile()),
            std::set<ntp_tiles::TileType>({ntp_tiles::TileType::kCustomLinks}));
}

class NewTabPageUtilFeatureOptimizationTest : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilFeatureOptimizationTest() {
    features().InitWithFeatures({ntp_features::kNtpFeatureOptimization}, {});
  }
};

IN_PROC_BROWSER_TEST_P(NewTabPageUtilFeatureOptimizationTest,
                       DisableModuleAutoRemoval) {
  // Arrange.
  const std::string module_id = ntp_modules::kGoogleCalendarModuleId;

  // Act.
  DisableModuleAutoRemoval(browser()->profile(), module_id);

  // Assert.
  const bool actual_value =
      browser()
          ->profile()
          ->GetPrefs()
          ->GetDict(ntp_prefs::kNtpModulesAutoRemovalDisabledDict)
          .FindBool(module_id)
          .value_or(false);
  EXPECT_TRUE(actual_value);
}

INSTANTIATE_TEST_SUITE_P(All, NewTabPageUtilBrowserTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageUtilEnableFlagBrowserTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageUtilDisableFlagBrowserTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    All,
    NewTabPageUtilTileTypesEnterpriseShortcutsDisabledBrowserTest,
    testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    All,
    NewTabPageUtilTileTypesEnterpriseShortcutsEnabledNoMixingBrowserTest,
    testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    All,
    NewTabPageUtilTileTypesEnterpriseShortcutsEnabledAllowMixingBrowserTest,
    testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageUtilFeatureOptimizationTest,
                         testing::Bool());
