// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/force_installed_test_base.h"
#include "base/memory/raw_ptr.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

// The extension ids used here should be valid extension ids.
const char ForceInstalledTestBase::kExtensionId1[] =
    "abcdefghijklmnopabcdefghijklmnop";
const char ForceInstalledTestBase::kExtensionId2[] =
    "bcdefghijklmnopabcdefghijklmnopa";
const char ForceInstalledTestBase::kExtensionName1[] = "name1";
const char ForceInstalledTestBase::kExtensionName2[] = "name2";
const char ForceInstalledTestBase::kExtensionUpdateUrl[] =
    "https://clients2.google.com/service/update2/crx";  // URL of Chrome Web
                                                        // Store backend.
const char ForceInstalledTestBase::kOffStoreUpdateUrl[] =
    "https://www.example.com/update2/crx";

ForceInstalledTestBase::ForceInstalledTestBase() = default;
ForceInstalledTestBase::~ForceInstalledTestBase() = default;

void ForceInstalledTestBase::SetUp() {
  policy_provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/false,
      /*is_first_policy_load_complete_return=*/false);

  auto policy_service = std::make_unique<policy::PolicyServiceImpl>(
      std::vector<
          raw_ptr<policy::ConfigurationPolicyProvider, VectorExperimental>>{
          &policy_provider_});
  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_->SetUp());
  profile_ = profile_manager_->CreateTestingProfile(
      "p1", nullptr, u"p1", 0, TestingProfile::TestingFactories(),
      /*is_supervised_profile=*/false, std::nullopt, std::move(policy_service));

  prefs_ = profile_->GetTestingPrefService();
  registry_ = ExtensionRegistry::Get(profile_);
  install_stage_tracker_ = InstallStageTracker::Get(profile_);
  force_installed_tracker_ =
      std::make_unique<ForceInstalledTracker>(registry_, profile_);
}

void ForceInstalledTestBase::SetupForceList(ExtensionOrigin origin) {
  base::Value::List list;
  const std::string update_url = origin == ExtensionOrigin::kWebStore
                                     ? kExtensionUpdateUrl
                                     : kOffStoreUpdateUrl;
  list.Append(base::StrCat({kExtensionId1, ";", update_url}));
  list.Append(base::StrCat({kExtensionId2, ";", update_url}));
  base::Value::Dict dict =
      base::Value::Dict()
          .Set(kExtensionId1,
               base::Value::Dict().Set(ExternalProviderImpl::kExternalUpdateUrl,
                                       update_url))
          .Set(kExtensionId2,
               base::Value::Dict().Set(ExternalProviderImpl::kExternalUpdateUrl,
                                       update_url));
  prefs_->SetManagedPref(pref_names::kInstallForceList, std::move(dict));

  EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(policy_provider_, IsFirstPolicyLoadComplete(testing::_))
      .WillRepeatedly(testing::Return(true));

  policy::PolicyMap map;
  map.Set("ExtensionInstallForcelist", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
          base::Value(std::move(list)), nullptr);
  policy_provider_.UpdateChromePolicy(map);
  base::RunLoop().RunUntilIdle();
}

void ForceInstalledTestBase::SetupEmptyForceList() {
  prefs_->SetManagedPref(pref_names::kInstallForceList, base::Value::Dict());

  EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(policy_provider_, IsFirstPolicyLoadComplete(testing::_))
      .WillRepeatedly(testing::Return(true));

  policy::PolicyMap map;
  policy_provider_.UpdateChromePolicy(std::move(map));
  base::RunLoop().RunUntilIdle();
}

scoped_refptr<const Extension> ForceInstalledTestBase::CreateNewExtension(
    const std::string& extension_name,
    const std::string& extension_id,
    const ForceInstalledTracker::ExtensionStatus& status) {
  auto ext = ExtensionBuilder(extension_name).SetID(extension_id).Build();
  switch (status) {
    case ForceInstalledTracker::ExtensionStatus::kPending:
    case ForceInstalledTracker::ExtensionStatus::kFailed:
      break;
    case ForceInstalledTracker::ExtensionStatus::kLoaded:
      registry()->AddEnabled(ext.get());
      force_installed_tracker()->OnExtensionLoaded(profile(), ext.get());
      break;
    case ForceInstalledTracker::ExtensionStatus::kReady:
      registry()->AddEnabled(ext.get());
      force_installed_tracker()->OnExtensionLoaded(profile(), ext.get());
      force_installed_tracker()->OnExtensionReady(profile(), ext.get());
  }
  return ext;
}

}  // namespace extensions
