// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/cloud_upload_prompt_prefs_handler.h"

#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"

namespace chromeos::cloud_upload {

class CloudUploadPromptPrefsHandlerTest : public policy::PolicyTest {
 public:
  CloudUploadPromptPrefsHandlerTest() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kUploadOfficeToCloud,
         chromeos::features::kUploadOfficeToCloudForEnterprise,
         chromeos::features::kUploadOfficeToCloudSync},
        {});
  }
  ~CloudUploadPromptPrefsHandlerTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  SyncServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating([](content::BrowserContext*)
                                              -> std::unique_ptr<KeyedService> {
                        return std::make_unique<syncer::TestSyncService>();
                      }));
                }));
  }

  void SetUpOnMainThread() override {
    policy::PolicyTest::SetUpOnMainThread();
    profile_policy_connector()->OverrideIsManagedForTesting(true);
  }

  // Sets the Microsoft Office cloud upload policy to `value` (should be one of
  // kAutomated, kAllowed, kDisallowed).
  void SetMicrosoftOfficeCloudUpload(const std::string& value) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies,
                                  policy::key::kMicrosoftOfficeCloudUpload,
                                  base::Value(value));
    provider_.UpdateChromePolicy(policies);
  }

  // Sets the Microsoft Office cloud upload policy to `value` (should be one of
  // kAutomated, kAllowed, kDisallowed).
  void SetGoogleWorkspaceCloudUpload(const std::string& value) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies,
                                  policy::key::kGoogleWorkspaceCloudUpload,
                                  base::Value(value));
    provider_.UpdateChromePolicy(policies);
  }

  // Verifies the syncing behavior of two preferences: `syncable_pref` and
  // `local_pref`. It sets `syncable_pref` to `true` and checks whether
  // `local_pref` is updated or not, depending on the `should_update` parameter.
  // The test assumes that both preferences are initially set to `false`.
  void TestPrefSyncing(const std::string& syncable_pref,
                       const std::string& local_pref,
                       bool should_update) {
    // Simulate a pref change from another device.
    profile()->GetPrefs()->SetBoolean(syncable_pref, true);
    EXPECT_EQ(should_update, profile()->GetPrefs()->GetBoolean(local_pref));
  }

  Profile* profile() { return browser()->profile(); }

  policy::ProfilePolicyConnector* profile_policy_connector() {
    return browser()->profile()->GetProfilePolicyConnector();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(CloudUploadPromptPrefsHandlerTest,
                       KeyedServiceRegistered) {
  std::vector<raw_ptr<DependencyNode, VectorExperimental>> nodes;
  const bool success = BrowserContextDependencyManager::GetInstance()
                           ->GetDependencyGraphForTesting()
                           .GetConstructionOrder(&nodes);
  EXPECT_TRUE(success);
  base::Contains(
      nodes, "CloudUploadPromptPrefsHandlerFactory",
      [](const DependencyNode* node) -> std::string_view {
        return static_cast<const KeyedServiceBaseFactory*>(node)->name();
      });
}

// Tests that the kOfficeFilesAlwaysMoveToDrive pref is synced when the
// Google Workspace cloud upload policy is set to "automated".
IN_PROC_BROWSER_TEST_F(CloudUploadPromptPrefsHandlerTest,
                       PrefsSyncedIfAutomated_GoogleDrive) {
  SetGoogleWorkspaceCloudUpload(kCloudUploadPolicyAutomated);

  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kOfficeFilesAlwaysMoveToDrive));
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kOfficeFilesAlwaysMoveToDriveSyncable));

  TestPrefSyncing(
      /*syncable_pref=*/prefs::kOfficeFilesAlwaysMoveToDriveSyncable,
      /*local_pref=*/prefs::kOfficeFilesAlwaysMoveToDrive,
      /*should_update=*/true);
}

// Tests that the kOfficeFilesAlwaysMoveToOneDrive pref is synced when the
// Microsoft Office cloud upload policy is set to "automated".
IN_PROC_BROWSER_TEST_F(CloudUploadPromptPrefsHandlerTest,
                       PrefsUpdatedIfAutomated_OneDrive) {
  SetMicrosoftOfficeCloudUpload(kCloudUploadPolicyAutomated);

  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kOfficeFilesAlwaysMoveToOneDrive));
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable));

  TestPrefSyncing(
      /*syncable_pref=*/prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable,
      /*local_pref=*/prefs::kOfficeFilesAlwaysMoveToOneDrive,
      /*should_update=*/true);
}

// Tests that kOfficeFilesAlwaysMoveToDriveSyncable is updated to reflect
// changes in kOfficeFilesAlwaysMoveToDrive, regardless of the cloud upload
// policy.
IN_PROC_BROWSER_TEST_F(CloudUploadPromptPrefsHandlerTest,
                       SyncablePrefsUpdatedIfLocalPrefsChange_GoogleDrive) {
  for (const std::string& cloud_upload_policy :
       {kCloudUploadPolicyAutomated, kCloudUploadPolicyAllowed,
        kCloudUploadPolicyDisallowed}) {
    SetGoogleWorkspaceCloudUpload(cloud_upload_policy);

    bool current_local_value =
        profile()->GetPrefs()->GetBoolean(prefs::kOfficeFilesAlwaysMoveToDrive);
    ASSERT_EQ(current_local_value,
              profile()->GetPrefs()->GetBoolean(
                  prefs::kOfficeFilesAlwaysMoveToDriveSyncable));
    // Change the local pref.
    profile()->GetPrefs()->SetBoolean(prefs::kOfficeFilesAlwaysMoveToDrive,
                                      !current_local_value);
    ASSERT_EQ(!current_local_value,
              profile()->GetPrefs()->GetBoolean(
                  prefs::kOfficeFilesAlwaysMoveToDriveSyncable));
  }
}

// Tests that kOfficeFilesAlwaysMoveToOneDriveSyncable is updated to reflect
// changes in kOfficeFilesAlwaysMoveToOneDrive, regardless of the cloud upload
// policy.
IN_PROC_BROWSER_TEST_F(CloudUploadPromptPrefsHandlerTest,
                       SyncablePrefsUpdatedIfLocalPrefsChange_OneDrive) {
  for (const std::string& cloud_upload_policy :
       {kCloudUploadPolicyAutomated, kCloudUploadPolicyAllowed,
        kCloudUploadPolicyDisallowed}) {
    SetMicrosoftOfficeCloudUpload(cloud_upload_policy);

    bool current_local_value = profile()->GetPrefs()->GetBoolean(
        prefs::kOfficeFilesAlwaysMoveToOneDrive);
    ASSERT_EQ(current_local_value,
              profile()->GetPrefs()->GetBoolean(
                  prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable));
    // Change the local pref.
    profile()->GetPrefs()->SetBoolean(prefs::kOfficeFilesAlwaysMoveToOneDrive,
                                      !current_local_value);
    ASSERT_EQ(!current_local_value,
              profile()->GetPrefs()->GetBoolean(
                  prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable));
  }
}

// Tests that the kOfficeFilesAlwaysMoveToDrive and
// kOfficeFilesAlwaysMoveToOneDrive prefs aren't synced when the profile isn't
// enterprise managed.
IN_PROC_BROWSER_TEST_F(CloudUploadPromptPrefsHandlerTest,
                       PrefsNotSyncedIfProfileNotEnterpriseManaged) {
  profile_policy_connector()->OverrideIsManagedForTesting(false);

  TestPrefSyncing(
      /*syncable_pref=*/prefs::kOfficeFilesAlwaysMoveToDriveSyncable,
      /*local_pref=*/prefs::kOfficeFilesAlwaysMoveToDrive,
      /*should_update=*/false);

  TestPrefSyncing(
      /*syncable_pref=*/prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable,
      /*local_pref=*/prefs::kOfficeFilesAlwaysMoveToOneDrive,
      /*should_update=*/false);
  ;
}

// Tests that the kOfficeFilesAlwaysMoveToDrive and
// kOfficeFilesAlwaysMoveToOneDrive prefs aren't synced if their respective
// Cloud Upload policies aren't set to "automated".
IN_PROC_BROWSER_TEST_F(CloudUploadPromptPrefsHandlerTest,
                       PrefsNotSyncedIfCloudUploadNotAutomated) {
  SetGoogleWorkspaceCloudUpload(kCloudUploadPolicyAllowed);
  TestPrefSyncing(
      /*syncable_pref=*/prefs::kOfficeFilesAlwaysMoveToDriveSyncable,
      /*local_pref=*/prefs::kOfficeFilesAlwaysMoveToDrive,
      /*should_update=*/false);

  SetMicrosoftOfficeCloudUpload(kCloudUploadPolicyDisallowed);
  TestPrefSyncing(
      /*syncable_pref=*/prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable,
      /*local_pref=*/prefs::kOfficeFilesAlwaysMoveToOneDrive,
      /*should_update=*/false);
  ;
}

// Tests that the kOfficeFilesAlwaysMoveToDrive pref is synced once the
// GoogleWorkspaceCloudUpload policy becomes "automated".
IN_PROC_BROWSER_TEST_F(CloudUploadPromptPrefsHandlerTest,
                       PrefsSyncedWhenCloudUploadBecomesAutomated) {
  SetMicrosoftOfficeCloudUpload(kCloudUploadPolicyDisallowed);
  SetGoogleWorkspaceCloudUpload(kCloudUploadPolicyAllowed);
  // Set the syncable pref before the policy change, but it shouldn't trigger
  // the sync.
  TestPrefSyncing(
      /*syncable_pref=*/prefs::kOfficeFilesAlwaysMoveToDriveSyncable,
      /*local_pref=*/prefs::kOfficeFilesAlwaysMoveToDrive,
      /*should_update=*/false);

  SetGoogleWorkspaceCloudUpload(kCloudUploadPolicyAutomated);
  EXPECT_EQ(
      profile()->GetPrefs()->GetBoolean(
          prefs::kOfficeFilesAlwaysMoveToDriveSyncable),
      profile()->GetPrefs()->GetBoolean(prefs::kOfficeFilesAlwaysMoveToDrive));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kOfficeFilesAlwaysMoveToDrive));
}

}  // namespace chromeos::cloud_upload
