// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"

#include "base/run_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_private_api = extensions::api::settings_private;
using extensions::settings_private::GeneratedPref;
using extensions::settings_private::SetPrefResult;
using settings_private_api::PrefObject;

namespace content_settings {
namespace {

int GetPrefValue(const PrefObject& pref_object) {
  if (pref_object.type != settings_private_api::PrefType::kNumber ||
      !pref_object.value->is_int()) {
    return -1;
  }
  return pref_object.value->GetInt();
}

int GetGeneratedPrefValue(Profile* profile) {
  PrefObject pref_object =
      GeneratedJavascriptOptimizerPref(profile).GetPrefObject();
  return GetPrefValue(pref_object);
}

extensions::settings_private::SetPrefResult SetPref(
    GeneratedJavascriptOptimizerPref& pref,
    const base::Value& value) {
  return pref.SetPref(&value);
}

// GeneratedPref::Observer which exposes method for waiting till generated pref
// has changed.
class TestObserver : public GeneratedPref::Observer {
 public:
  TestObserver() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
  }

  ~TestObserver() override = default;

  void WaitForGeneratedPrefChange() {
    if (was_pref_changed_) {
      return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnGeneratedPrefChanged(const std::string&) override {
    was_pref_changed_ = true;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  bool was_pref_changed_ = false;
  base::OnceClosure quit_closure_;
};

}  // anonymous namespace

class GeneratedJavascriptOptimizerPrefTest : public testing::Test {
 public:
  GeneratedJavascriptOptimizerPrefTest() = default;

  void SetUp() override {
    testing::Test::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(profile_.get());
  }

  TestingProfile* profile() { return profile_.get(); }
  PrefService* prefs() { return profile()->GetPrefs(); }

  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
};

void EnableFeature(base::test::ScopedFeatureList* feature_list) {
  feature_list
      ->InitWithFeatures(/*enabled_features=*/
                         {content_settings::features::
                              kBlockV8OptimizerOnUnfamiliarSitesSetting,
                          ::features::kProcessSelectionDeferringConditions},
                         /*disabled_features=*/{});
}

TEST_F(GeneratedJavascriptOptimizerPrefTest, GetPrefObject_FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  EnableFeature(&scoped_feature_list);

  const struct TestCase {
    ContentSetting content_setting;
    bool pref_blocked_for_unfamiliar_sites;
    JavascriptOptimizerSetting expected_setting;
  } kTestCases[] = {
      {ContentSetting::CONTENT_SETTING_ALLOW, false,
       JavascriptOptimizerSetting::kAllowed},
      {ContentSetting::CONTENT_SETTING_ALLOW, true,
       JavascriptOptimizerSetting::kBlockedForUnfamiliarSites},
      {ContentSetting::CONTENT_SETTING_BLOCK, false,
       JavascriptOptimizerSetting::kBlocked},
      {ContentSetting::CONTENT_SETTING_BLOCK, true,
       JavascriptOptimizerSetting::kBlocked},
  };

  for (const auto& test_case : kTestCases) {
    host_content_settings_map()->SetDefaultContentSetting(
        ContentSettingsType::JAVASCRIPT_OPTIMIZER, test_case.content_setting);
    prefs()->SetBoolean(prefs::kJavascriptOptimizerBlockedForUnfamiliarSites,
                        test_case.pref_blocked_for_unfamiliar_sites);
    EXPECT_EQ(static_cast<int>(test_case.expected_setting),
              GetGeneratedPrefValue(profile()));
  }
}

TEST_F(GeneratedJavascriptOptimizerPrefTest, GetPrefObject_FeatureDisabled) {
  host_content_settings_map()->SetDefaultContentSetting(
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_ALLOW);
  prefs()->SetBoolean(prefs::kJavascriptOptimizerBlockedForUnfamiliarSites,
                      true);

  std::vector<base::test::FeatureRef> test_cases = {
      content_settings::features::kBlockV8OptimizerOnUnfamiliarSitesSetting,
      ::features::kProcessSelectionDeferringConditions};
  for (const base::test::FeatureRef& feature : test_cases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(*feature);
    EXPECT_EQ(static_cast<int>(JavascriptOptimizerSetting::kAllowed),
              GetGeneratedPrefValue(profile()));
  }
}

// Test potential future scenario where
// kJavascriptOptimizerBlockedForUnfamiliarSites is updated by
// non generated-pref code.
TEST_F(GeneratedJavascriptOptimizerPrefTest,
       GetPrefObject_PrefChangedExternally) {
  base::test::ScopedFeatureList scoped_feature_list;
  EnableFeature(&scoped_feature_list);

  prefs()->SetBoolean(prefs::kJavascriptOptimizerBlockedForUnfamiliarSites,
                      false);
  EXPECT_EQ(static_cast<int>(JavascriptOptimizerSetting::kAllowed),
            GetGeneratedPrefValue(profile()));

  GeneratedJavascriptOptimizerPref pref(profile());
  TestObserver observer;
  pref.AddObserver(&observer);

  prefs()->SetBoolean(prefs::kJavascriptOptimizerBlockedForUnfamiliarSites,
                      true);
  observer.WaitForGeneratedPrefChange();
  EXPECT_EQ(
      static_cast<int>(JavascriptOptimizerSetting::kBlockedForUnfamiliarSites),
      GetGeneratedPrefValue(profile()));
}

TEST_F(GeneratedJavascriptOptimizerPrefTest, GetPrefObject_SafeBrowsingOff) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  PrefObject pref_object =
      GeneratedJavascriptOptimizerPref(profile()).GetPrefObject();
  EXPECT_EQ(static_cast<int>(JavascriptOptimizerSetting::kAllowed),
            GetPrefValue(pref_object));
  EXPECT_EQ(settings_private_api::Enforcement::kEnforced,
            pref_object.enforcement);
  EXPECT_EQ(settings_private_api::ControlledBy::kSafeBrowsingOff,
            pref_object.controlled_by);
}

TEST_F(GeneratedJavascriptOptimizerPrefTest,
       GetPrefObject_DisableForUnfamiliar_ThenSafeBrowsingOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  EnableFeature(&scoped_feature_list);
  prefs()->SetBoolean(prefs::kJavascriptOptimizerBlockedForUnfamiliarSites,
                      true);
  EXPECT_EQ(
      static_cast<int>(JavascriptOptimizerSetting::kBlockedForUnfamiliarSites),
      GetGeneratedPrefValue(profile()));

  // Disable safe browsing after user has disabled v8-optimizers for unfamiliar
  // sites.
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  PrefObject pref_object =
      GeneratedJavascriptOptimizerPref(profile()).GetPrefObject();
  EXPECT_EQ(static_cast<int>(JavascriptOptimizerSetting::kAllowed),
            GetPrefValue(pref_object));
  EXPECT_EQ(settings_private_api::Enforcement::kEnforced,
            pref_object.enforcement);
  EXPECT_EQ(settings_private_api::ControlledBy::kSafeBrowsingOff,
            pref_object.controlled_by);
}

TEST_F(GeneratedJavascriptOptimizerPrefTest, GetPrefObject_SafeBrowsingOn) {
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  PrefObject pref_object =
      GeneratedJavascriptOptimizerPref(profile()).GetPrefObject();
  EXPECT_EQ(settings_private_api::Enforcement::kNone, pref_object.enforcement);
}

TEST_F(GeneratedJavascriptOptimizerPrefTest, GetPrefObject_Policy) {
  ContentSettingsRegistry::GetInstance();
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kManagedDefaultJavaScriptOptimizerSetting,
      base::Value(ContentSetting::CONTENT_SETTING_BLOCK));

  PrefObject pref_object =
      GeneratedJavascriptOptimizerPref(profile()).GetPrefObject();
  EXPECT_EQ(settings_private_api::Enforcement::kEnforced,
            pref_object.enforcement);
  EXPECT_EQ(settings_private_api::ControlledBy::kDevicePolicy,
            pref_object.controlled_by);
}

TEST_F(GeneratedJavascriptOptimizerPrefTest,
       GetPrefObject_PolicyDisablesFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  EnableFeature(&scoped_feature_list);
  // Could be set by the user via chrome://settings prior to policy being set by
  // administrator.
  prefs()->SetBoolean(prefs::kJavascriptOptimizerBlockedForUnfamiliarSites,
                      true);

  ContentSettingsRegistry::GetInstance();
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kManagedDefaultJavaScriptOptimizerSetting,
      base::Value(ContentSetting::CONTENT_SETTING_ALLOW));

  EXPECT_EQ(static_cast<int>(JavascriptOptimizerSetting::kAllowed),
            GetGeneratedPrefValue(profile()));
}

TEST_F(GeneratedJavascriptOptimizerPrefTest, SetPrefResult) {
  const struct TestCase {
    JavascriptOptimizerSetting setting;
    ContentSetting expected_content_setting;
    bool expected_pref_blocked_for_unfamiliar_sites;
  } kTestCases[] = {
      {JavascriptOptimizerSetting::kAllowed,
       ContentSetting::CONTENT_SETTING_ALLOW, false},
      {JavascriptOptimizerSetting::kBlockedForUnfamiliarSites,
       ContentSetting::CONTENT_SETTING_ALLOW, true},
      {JavascriptOptimizerSetting::kBlocked,
       ContentSetting::CONTENT_SETTING_BLOCK, false},
  };

  for (const auto& test_case : kTestCases) {
    GeneratedJavascriptOptimizerPref pref(profile());
    SetPrefResult result =
        SetPref(pref, base::Value(static_cast<int>(test_case.setting)));
    EXPECT_EQ(result, SetPrefResult::SUCCESS);
    EXPECT_EQ(host_content_settings_map()->GetDefaultContentSetting(
                  ContentSettingsType::JAVASCRIPT_OPTIMIZER),
              test_case.expected_content_setting);
    EXPECT_EQ(prefs()->GetBoolean(
                  prefs::kJavascriptOptimizerBlockedForUnfamiliarSites),
              test_case.expected_pref_blocked_for_unfamiliar_sites);
  }
}

TEST_F(GeneratedJavascriptOptimizerPrefTest, SetPref_NotInt) {
  GeneratedJavascriptOptimizerPref pref(profile());
  SetPrefResult result = SetPref(pref, base::Value("not an int"));
  EXPECT_EQ(result, SetPrefResult::PREF_TYPE_MISMATCH);
}

TEST_F(GeneratedJavascriptOptimizerPrefTest, SetPref_OutOfRange) {
  GeneratedJavascriptOptimizerPref pref(profile());
  SetPrefResult result = SetPref(pref, base::Value(100));
  EXPECT_EQ(result, SetPrefResult::PREF_TYPE_MISMATCH);
}

}  // namespace content_settings
