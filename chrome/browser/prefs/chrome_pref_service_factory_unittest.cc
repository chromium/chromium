// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/chrome_pref_service_factory.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ChromePrefServiceFactoryTestBase : public testing::Test {
 public:
  ChromePrefServiceFactoryTestBase()
      : pref_registry_(
            base::MakeRefCounted<user_prefs::PrefRegistrySyncable>()) {
    EXPECT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_ = std::make_unique<TestingProfile>(data_dir_.GetPath());
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> BuildPrefService() {
    return chrome_prefs::CreateProfilePrefs(
        profile_->GetPath(), /*validation_delegate=*/mojo::NullRemote(),
        /*policy_service=*/
        g_browser_process->browser_policy_connector()->GetPolicyService(),
        /*supervised_user_settings=*/nullptr,
        /*content_filters_service=*/nullptr,
        /*extension_prefs=*/nullptr, pref_registry_,
        /*connector=*/g_browser_process->browser_policy_connector(),
        /*async=*/true, task_environment_.GetMainThreadTaskRunner().get(),
        /*os_crypt_async=*/nullptr);
  }

  base::FilePath AccountPreferencesFilePath() const {
    return data_dir_.GetPath().Append(chrome::kAccountPreferencesFilename);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  std::unique_ptr<TestingProfile> profile_;
};

class ChromePrefServiceFactoryTamperedPrefTest
    : public ChromePrefServiceFactoryTestBase {
 public:
  ChromePrefServiceFactoryTamperedPrefTest() {
    pref_registry_->RegisterListPref(user_prefs::kTrackedPreferencesReset);
  }
};

TEST_F(ChromePrefServiceFactoryTamperedPrefTest,
       GetTamperedPrefListEmptyAndPopulated) {
  PrefService* pref_service = profile_->GetPrefs();

  EXPECT_TRUE(chrome_prefs::GetTamperedPrefList(profile_.get()).empty());

  base::Value::List tampered_list;
  tampered_list.Append("pref.path.one");
  tampered_list.Append("pref.path.two");
  pref_service->SetList(user_prefs::kTrackedPreferencesReset,
                        std::move(tampered_list));

  const base::Value::List& retrieved_list =
      chrome_prefs::GetTamperedPrefList(profile_.get());
  EXPECT_EQ(2U, retrieved_list.size());
  EXPECT_EQ("pref.path.one", retrieved_list[0].GetString());
  EXPECT_EQ("pref.path.two", retrieved_list[1].GetString());
}

TEST_F(ChromePrefServiceFactoryTamperedPrefTest,
       ClearTamperedPrefListClearsPref) {
  PrefService* pref_service = profile_->GetPrefs();

  base::Value::List tampered_list;
  tampered_list.Append("pref.path.to.clear");
  pref_service->SetList(user_prefs::kTrackedPreferencesReset,
                        std::move(tampered_list));
  EXPECT_FALSE(chrome_prefs::GetTamperedPrefList(profile_.get()).empty());

  chrome_prefs::ClearTamperedPrefList(profile_.get());

  EXPECT_TRUE(chrome_prefs::GetTamperedPrefList(profile_.get()).empty());
}

#if BUILDFLAG(IS_ANDROID)

class ChromePrefServiceFactoryTestWithMigrateAccountPrefsDisabled
    : public ChromePrefServiceFactoryTestBase {
 public:
  ChromePrefServiceFactoryTestWithMigrateAccountPrefsDisabled() {
    feature_list_.InitAndDisableFeature(syncer::kMigrateAccountPrefs);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ChromePrefServiceFactoryTestWithMigrateAccountPrefsDisabled,
       ShouldNotRemoveAccountPrefsFile) {
  // Simulate a pre-existing account preferences file.
  ASSERT_TRUE(base::WriteFile(AccountPreferencesFilePath(), "{}"));

  BuildPrefService();
  // Wait for any tasks posted to the IO to finish.
  base::RunLoop run_loop;
  content::GetIOThreadTaskRunner()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Account prefs file should not be removed.
  EXPECT_TRUE(base::PathExists(AccountPreferencesFilePath()));
}

class ChromePrefServiceFactoryTestWithMigrateAccountPrefsEnabled
    : public ChromePrefServiceFactoryTestBase {
 public:
  ChromePrefServiceFactoryTestWithMigrateAccountPrefsEnabled()
      : feature_list_(syncer::kMigrateAccountPrefs) {}

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ChromePrefServiceFactoryTestWithMigrateAccountPrefsEnabled,
       ShouldRemoveAccountPrefsFile) {
  // Simulate a pre-existing account preferences file.
  ASSERT_TRUE(base::WriteFile(AccountPreferencesFilePath(), "{}"));

  BuildPrefService();
  // Wait for the posted task on the IO thread to delete the file finish.
  base::RunLoop run_loop;
  content::GetIOThreadTaskRunner()->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Account prefs file should have been removed.
  EXPECT_FALSE(base::PathExists(AccountPreferencesFilePath()));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
