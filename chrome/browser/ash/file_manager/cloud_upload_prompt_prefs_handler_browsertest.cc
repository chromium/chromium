// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/cloud_upload_prompt_prefs_handler.h"

#include <tuple>

#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"

namespace chromeos::cloud_upload {

namespace {
// Extracts the last part of the preference name, which represents the unique
// identifier, and returns it converted to CamelCase. Expects a
// period-separated string in the "filebrowser.office.[unique_name]" format.
//
// E.g. "filebrowser.office.always_move_to_drive" returns "AlwaysMoveToDrive".
std::string ConvertPrefNameToCamelCase(const std::string& pref_name) {
  std::string last_part =
      base::SplitString(pref_name, ".", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL)
          .back();
  std::string result;
  bool capitalize_next = true;
  for (char c : last_part) {
    if (c == '_') {
      capitalize_next = true;
    } else if (isalnum(c)) {
      result += capitalize_next ? std::toupper(c) : c;
      capitalize_next = false;
    }
  }
  return result;
}
}  // namespace

using ash::cloud_upload::CloudProvider;

class CloudUploadPromptPrefsHandlerTestBase : public InProcessBrowserTest {
 public:
  CloudUploadPromptPrefsHandlerTestBase() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kUploadOfficeToCloud,
         chromeos::features::kUploadOfficeToCloudForEnterprise,
         chromeos::features::kUploadOfficeToCloudSync},
        {});
  }
  ~CloudUploadPromptPrefsHandlerTestBase() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // The service is created only for managed profiles.
    TestingProfile::Builder profile_builder;
    profile_builder.OverridePolicyConnectorIsManagedForTesting(true);
    profile_ = profile_builder.Build();
  }

  void TearDownOnMainThread() override { profile_.reset(); }

  Profile* profile() { return profile_.get(); }

  policy::ProfilePolicyConnector* profile_policy_connector() {
    return profile_->GetProfilePolicyConnector();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

IN_PROC_BROWSER_TEST_F(CloudUploadPromptPrefsHandlerTestBase,
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

class CloudUploadPromptPrefsHandlerTest
    : public CloudUploadPromptPrefsHandlerTestBase,
      public ::testing::WithParamInterface<
          std::tuple<CloudProvider,
                     /*local_pref*/ std::string,
                     /*syncable_pref*/ std::string>> {
 public:
  // Converts a TestParamInfo object to a human-readable string.
  // The string is derived from the `local_pref` member of the
  // parameter.
  //
  // We don't use `cloud_provider` or `syncable_pref` because the former is
  // redundant and the latter is simply the `local_pref` with a 'syncable'
  // suffix.
  static std::string ParamToString(
      const ::testing::TestParamInfo<ParamType>& info) {
    auto [cloud_provider, local_pref, syncable_pref] = info.param;
    return ConvertPrefNameToCamelCase(local_pref);
  }

  CloudUploadPromptPrefsHandlerTest() = default;
  ~CloudUploadPromptPrefsHandlerTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    CloudUploadPromptPrefsHandlerTestBase::SetUpBrowserContextKeyedServices(
        context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindOnce(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
  }

  // Sets the cloud upload policy, based on the `CloudProvider` parameter, to
  // `value` (should be one of kAutomated, kAllowed, kDisallowed).
  void SetCloudUploadPolicy(const std::string& value) {
    const CloudProvider cloud_provider = std::get<0>(GetParam());
    switch (cloud_provider) {
      case ash::cloud_upload::CloudProvider::kGoogleDrive:
        profile()->GetPrefs()->SetString(prefs::kGoogleWorkspaceCloudUpload,
                                         value);
        break;
      case ash::cloud_upload::CloudProvider::kOneDrive:
        profile()->GetPrefs()->SetString(prefs::kMicrosoftOfficeCloudUpload,
                                         value);
        break;
      case ash::cloud_upload::CloudProvider::kNone:
      case ash::cloud_upload::CloudProvider::kUnknown:
        NOTREACHED();
    }
  }

  // Verifies the syncing behavior of two preferences: `syncable_pref` and
  // `local_pref`. It sets `syncable_pref` to `true` and checks whether
  // `local_pref` is updated or not, depending on the `should_update`
  // parameter. The test assumes that both preferences are initially set to
  // `false`.
  void TestPrefSyncing(const std::string& syncable_pref,
                       const std::string& local_pref,
                       bool should_update) {
    // Simulate a pref change from another device.
    profile()->GetPrefs()->SetBoolean(syncable_pref, true);
    EXPECT_EQ(should_update, profile()->GetPrefs()->GetBoolean(local_pref))
        << "Pref " << local_pref << " should " << (should_update ? "" : "not ")
        << "have been updated";
  }

  const std::string& local_pref() { return std::get<1>(GetParam()); }

  const std::string& syncable_pref() { return std::get<2>(GetParam()); }
};

// Tests that local prefs are synced when the corresponding Google Workspace or
// Microsoft Office cloud upload policy is set to "automated".
IN_PROC_BROWSER_TEST_P(CloudUploadPromptPrefsHandlerTest,
                       PrefsSyncedIfAutomated) {
  SetCloudUploadPolicy(kCloudUploadPolicyAutomated);

  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(local_pref()))
      << "Pref " << local_pref()
      << " is true, but it should initially be false";
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(syncable_pref()))
      << "Pref " << syncable_pref()
      << " is true, but it should initially be false";
  TestPrefSyncing(syncable_pref(), local_pref(),
                  /*should_update=*/true);
}

// Tests that syncable prefs are updated to reflect changes in their local
// counterparts, regardless of the cloud upload policy.
IN_PROC_BROWSER_TEST_P(CloudUploadPromptPrefsHandlerTest,
                       SyncablePrefsUpdatedIfLocalPrefsChange) {
  for (const std::string& cloud_upload_policy :
       {kCloudUploadPolicyAutomated, kCloudUploadPolicyAllowed,
        kCloudUploadPolicyDisallowed}) {
    SetCloudUploadPolicy(cloud_upload_policy);

    SCOPED_TRACE(::testing::Message()
                 << "Testing local pref: " << local_pref()
                 << " and syncable pref: " << syncable_pref()
                 << " with policy: " << cloud_upload_policy);

    bool current_local_value = profile()->GetPrefs()->GetBoolean(local_pref());
    bool new_value = !current_local_value;
    ASSERT_EQ(current_local_value,
              profile()->GetPrefs()->GetBoolean(syncable_pref()))
        << "Pref: " << syncable_pref()
        << " should initially be equal to the local pref: " << local_pref();
    // Change the local pref.
    profile()->GetPrefs()->SetBoolean(local_pref(), new_value);
    ASSERT_EQ(new_value, profile()->GetPrefs()->GetBoolean(syncable_pref()))
        << "Pref: " << syncable_pref() << " should have been updated to "
        << (new_value ? " true" : " false");
  }
}

// Tests that the local prefs aren't synced when the profile isn't
// enterprise managed.
IN_PROC_BROWSER_TEST_P(CloudUploadPromptPrefsHandlerTest,
                       PrefsNotSyncedIfProfileNotEnterpriseManaged) {
  profile_policy_connector()->OverrideIsManagedForTesting(false);
  TestPrefSyncing(syncable_pref(), local_pref(), /*should_update=*/false);
}

// Tests that the prefs aren't synced if the respective Cloud Upload policies
// aren't set to "automated".
IN_PROC_BROWSER_TEST_P(CloudUploadPromptPrefsHandlerTest,
                       PrefsNotSyncedIfCloudUploadNotAutomated) {
  for (const std::string& cloud_upload_policy :
       {kCloudUploadPolicyAllowed, kCloudUploadPolicyDisallowed}) {
    SetCloudUploadPolicy(cloud_upload_policy);
    TestPrefSyncing(syncable_pref(), local_pref(),
                    /*should_update=*/false);
  }
}

// Tests that the prefs are synced once the Cloud Upload policy is set to
// "automated".
IN_PROC_BROWSER_TEST_P(CloudUploadPromptPrefsHandlerTest,
                       PrefsSyncedWhenCloudUploadBecomesAutomated) {
  SetCloudUploadPolicy(kCloudUploadPolicyAllowed);
  // Set the syncable pref before the policy change, which shouldn't trigger the
  // sync yet.
  TestPrefSyncing(syncable_pref(), local_pref(),
                  /*should_update=*/false);

  SetCloudUploadPolicy(kCloudUploadPolicyAutomated);
  EXPECT_EQ(profile()->GetPrefs()->GetBoolean(syncable_pref()),
            profile()->GetPrefs()->GetBoolean(local_pref()))
      << "Pref: " << local_pref()
      << " should be equal to the syncable pref: " << syncable_pref();
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(local_pref()))
      << "Pref: " << local_pref() << " is false, but should be true.";
}

INSTANTIATE_TEST_SUITE_P(
    CloudUploadPrefsSync,
    CloudUploadPromptPrefsHandlerTest,
    testing::Values(
        std::make_tuple(CloudProvider::kGoogleDrive,
                        prefs::kOfficeFilesAlwaysMoveToDrive,
                        prefs::kOfficeFilesAlwaysMoveToDriveSyncable),
        std::make_tuple(CloudProvider::kGoogleDrive,
                        prefs::kOfficeMoveConfirmationShownForDrive,
                        prefs::kOfficeMoveConfirmationShownForDriveSyncable),
        std::make_tuple(
            CloudProvider::kGoogleDrive,
            prefs::kOfficeMoveConfirmationShownForLocalToDrive,
            prefs::kOfficeMoveConfirmationShownForLocalToDriveSyncable),
        std::make_tuple(
            CloudProvider::kGoogleDrive,
            prefs::kOfficeMoveConfirmationShownForCloudToDrive,
            prefs::kOfficeMoveConfirmationShownForCloudToDriveSyncable),
        std::make_tuple(CloudProvider::kOneDrive,
                        prefs::kOfficeFilesAlwaysMoveToOneDrive,
                        prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable),
        std::make_tuple(CloudProvider::kOneDrive,
                        prefs::kOfficeMoveConfirmationShownForOneDrive,
                        prefs::kOfficeMoveConfirmationShownForOneDriveSyncable),
        std::make_tuple(
            CloudProvider::kOneDrive,
            prefs::kOfficeMoveConfirmationShownForLocalToOneDrive,
            prefs::kOfficeMoveConfirmationShownForLocalToOneDriveSyncable),
        std::make_tuple(
            CloudProvider::kOneDrive,
            prefs::kOfficeMoveConfirmationShownForCloudToOneDrive,
            prefs::kOfficeMoveConfirmationShownForCloudToOneDriveSyncable)),
    &CloudUploadPromptPrefsHandlerTest::ParamToString);

}  // namespace chromeos::cloud_upload
