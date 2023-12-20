// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/preferences/public/cpp/tracked/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPrefKey[] = "pref_key";
const char kPrefValue[] = "pref_val";
const char kDefaultPrefValue[] = "default_pref_value";

void WriteTestPrefToFile(base::FilePath path) {
  base::Value::Dict pref_dict = base::Value::Dict().Set(kPrefKey, kPrefValue);
  JSONFileValueSerializer serializer(path);
  ASSERT_TRUE(serializer.Serialize(pref_dict));
}

}  // namespace

namespace chrome_prefs {

class ChromePrefServiceFactoryTest : public testing::Test {
 public:
  ChromePrefServiceFactoryTest()
      : pref_store_(base::MakeRefCounted<DefaultPrefStore>()),
        pref_registry_(
            base::MakeRefCounted<user_prefs::PrefRegistrySyncable>()) {}
  ~ChromePrefServiceFactoryTest() override = default;

 protected:
  void SetUp() override {
    install_attributes_ = std::make_unique<ash::StubInstallAttributes>();
    ash::InstallAttributes::SetForTesting(install_attributes_.get());
    policy_provider_ = std::make_unique<
        testing::NiceMock<policy::MockConfigurationPolicyProvider>>();
    policy_provider_->SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    std::vector<
        raw_ptr<policy::ConfigurationPolicyProvider, VectorExperimental>>
        providers = {policy_provider_.get()};
    policy_service_ = std::make_unique<policy::PolicyServiceImpl>(providers);

    policy::BrowserPolicyConnectorBase::SetPolicyServiceForTesting(
        policy_service_.get());

    // Create a temp user directory.
    ASSERT_TRUE(user_dir_.CreateUniqueTempDir());
    user_data_override_ = std::make_unique<base::ScopedPathOverride>(
        chrome::DIR_USER_DATA, user_dir_.GetPath(), true, true);
    // Create the profile directory
    profile_path_ = user_dir_.GetPath().Append(FILE_PATH_LITERAL("u-test"));
    EXPECT_TRUE(base::CreateDirectory(profile_path_));

    // Register the fake pref that will be written in the standalone browser
    // preferences file (see `WriteTestPrefToFile`).
    pref_registry_->RegisterStringPref(kPrefKey, kDefaultPrefValue);
  }

  void TearDown() override {
    g_browser_process->browser_policy_connector()->Shutdown();
    policy_service_.reset();
    ash::InstallAttributes::ShutdownForTesting();
  }

  const base::FilePath& profile_path() const { return profile_path_; }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> GetProfilePrefs(
      const base::FilePath& profile_path) {
    std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service =
        chrome_prefs::CreateProfilePrefs(
            profile_path,
            mojo::PendingRemote<
                prefs::mojom::TrackedPreferenceValidationDelegate>(),
            policy_service_.get(), nullptr, pref_store_, pref_registry_,
            g_browser_process->browser_policy_connector(), false,
            task_environment_.GetMainThreadTaskRunner());
    // Run the message loop to process the async request which removes the
    // obsolete standalone browser pref file.
    task_environment_.RunUntilIdle();

    return pref_service;
  }

 private:
  base::ScopedTempDir user_dir_;
  std::unique_ptr<base::ScopedPathOverride> user_data_override_;
  base::FilePath profile_path_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ash::StubInstallAttributes> install_attributes_;
  std::unique_ptr<policy::PolicyServiceImpl> policy_service_;
  std::unique_ptr<policy::MockConfigurationPolicyProvider> policy_provider_;
  scoped_refptr<DefaultPrefStore> pref_store_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
};

// Verify that the preferences are not read from the wrong standalone browser
// preferences file (outside the profile directory). See b/300645795.
TEST_F(ChromePrefServiceFactoryTest,
       PrefsNotReadFromDeprecatedStandaloneBrowserFile) {
  base::FilePath user_data_directory;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory));
  base::FilePath standalone_browser_preferences_path =
      user_data_directory.Append(
          FILE_PATH_LITERAL("standalone_browser_preferences.json"));

  WriteTestPrefToFile(standalone_browser_preferences_path);

  auto pref_service = GetProfilePrefs(profile_path());
  ASSERT_TRUE(pref_service);

  const PrefService::Preference* pref = pref_service->FindPreference(kPrefKey);
  ASSERT_TRUE(pref);
  const base::Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  base::ExpectStringValue(kDefaultPrefValue, *value);

  // The file should be removed because it's not at the the correct location.
  EXPECT_FALSE(base::PathExists(standalone_browser_preferences_path));
}

// Verify that the preferences are read from the standalone browser file in the
// profile directory.
TEST_F(ChromePrefServiceFactoryTest,
       PrefsAreReadFromTheProfileStandaloneBrowserFile) {
  WriteTestPrefToFile(profile_path().Append(
      FILE_PATH_LITERAL("standalone_browser_preferences.json")));

  auto pref_service = GetProfilePrefs(profile_path());
  ASSERT_TRUE(pref_service);

  const PrefService::Preference* pref = pref_service->FindPreference(kPrefKey);
  ASSERT_TRUE(pref);
  const base::Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  base::ExpectStringValue(kPrefValue, *value);
}

}  // namespace chrome_prefs
