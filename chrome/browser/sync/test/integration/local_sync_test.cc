// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"

// The local sync backend is currently only supported on Windows, Mac, Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace {

using syncer::SyncServiceImpl;

constexpr char kTestPassphrase[] = "hunter2";

class SyncTransportActiveChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncTransportActiveChecker(SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync transport to become active";
    return service()->GetTransportState() ==
           syncer::SyncService::TransportState::ACTIVE;
  }
};

// This test verifies some basic functionality of local sync, used for roaming
// profiles (enterprise use-case).
class LocalSyncTest : public InProcessBrowserTest {
 public:
  LocalSyncTest(const LocalSyncTest&) = delete;
  LocalSyncTest& operator=(const LocalSyncTest&) = delete;

 protected:
  LocalSyncTest() {
    EXPECT_TRUE(local_sync_backend_dir_.CreateUniqueTempDir());
  }

  ~LocalSyncTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // By default on Window OS local sync backend uses roaming profile. It can
    // lead to problems if some tests run simultaneously and use the same
    // roaming profile.
    base::FilePath file = local_sync_backend_dir_.GetPath().Append(
        FILE_PATH_LITERAL("profile.pb"));
    command_line->AppendSwitchASCII(switches::kLocalSyncBackendDir,
                                    file.MaybeAsASCII());
    command_line->AppendSwitch(switches::kEnableLocalSyncBackend);
    command_line->AppendSwitchASCII(syncer::kSyncDeferredStartupTimeoutSeconds,
                                    "0");
  }

 private:
  base::ScopedTempDir local_sync_backend_dir_;
};

IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldStart) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->profile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  EXPECT_TRUE(service->IsLocalSyncEnabled());
  EXPECT_FALSE(service->IsSyncFeatureEnabled());
  EXPECT_FALSE(service->IsSyncFeatureActive());
  EXPECT_FALSE(service->GetUserSettings()->IsInitialSyncFeatureSetupComplete());

  // Verify that the expected set of data types successfully started up.
  // If this test fails after adding a new data type, carefully consider whether
  // the type should be enabled in Local Sync mode, i.e. for roaming profiles on
  // Windows.
  syncer::DataTypeSet expected_active_data_types = {
      syncer::BOOKMARKS,
      syncer::READING_LIST,
      syncer::PREFERENCES,
      syncer::PASSWORDS,
      syncer::AUTOFILL_PROFILE,
      syncer::AUTOFILL,
      syncer::THEMES,
      syncer::EXTENSIONS,
      syncer::SAVED_TAB_GROUP,
      syncer::SEARCH_ENGINES,
      syncer::SESSIONS,
      syncer::APPS,
      syncer::APP_SETTINGS,
      syncer::EXTENSION_SETTINGS,
      syncer::DEVICE_INFO,
      syncer::PRIORITY_PREFERENCES,
      syncer::WEBAUTHN_CREDENTIAL,
      syncer::WEB_APPS,
      syncer::NIGORI};

  if (base::FeatureList::IsEnabled(syncer::kSyncAutofillWalletCredentialData)) {
    expected_active_data_types.Put(syncer::AUTOFILL_WALLET_CREDENTIAL);
  }

  // The dictionary is currently only synced on Windows and Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  expected_active_data_types.Put(syncer::DICTIONARY);
#endif
  EXPECT_EQ(service->GetActiveDataTypes(), expected_active_data_types);

  // Verify certain features are disabled.
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::USER_CONSENTS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::USER_EVENTS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SECURITY_EVENTS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SEND_TAB_TO_SELF));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SEND_TAB_TO_SELF));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::HISTORY));
}

IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldHonorSelectedTypes) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->profile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  ASSERT_TRUE(service->IsLocalSyncEnabled());
  ASSERT_FALSE(service->IsSyncFeatureEnabled());
  ASSERT_TRUE(service->GetActiveDataTypes().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(service->GetActiveDataTypes().Has(syncer::PASSWORDS));

  service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kPasswords});

  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::PASSWORDS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::BOOKMARKS));
}

// Setting up a custom passphrase is arguably meaningless for local sync, but it
// has been allowed historically.
IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldSupportCustomPassphrase) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->profile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  ASSERT_TRUE(service->IsLocalSyncEnabled());
  ASSERT_FALSE(service->IsSyncFeatureEnabled());

  service->GetUserSettings()->SetEncryptionPassphrase(kTestPassphrase);

  EXPECT_TRUE(PassphraseAcceptedChecker(service).Wait());
}

IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldReportNoLocalOnlyData) {
  SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->profile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());
  ASSERT_TRUE(service->IsLocalSyncEnabled());
  ASSERT_FALSE(service->HasSyncConsent());

  base::test::TestFuture<absl::flat_hash_map<syncer::DataType, size_t>>
      types_with_unsynced_data;
  service->GetTypesWithUnsyncedData({syncer::BOOKMARKS},
                                    types_with_unsynced_data.GetCallback());
  EXPECT_TRUE(types_with_unsynced_data.Get().empty());

  base::test::TestFuture<
      std::map<syncer::DataType, syncer::LocalDataDescription>>
      descriptions;
  service->GetLocalDataDescriptions({syncer::BOOKMARKS},
                                    descriptions.GetCallback());
  EXPECT_TRUE(descriptions.Get().empty());
}

}  // namespace

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
