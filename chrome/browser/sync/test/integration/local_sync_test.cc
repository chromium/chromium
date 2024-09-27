// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "crypto/ec_private_key.h"

namespace {

using syncer::SyncServiceImpl;

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

// The local sync backend is currently only supported on Windows, Mac, Linux,
// and Lacros.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
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
      syncer::WEB_APPS,
      syncer::NIGORI};

  if (base::FeatureList::IsEnabled(power_bookmarks::kPowerBookmarkBackend)) {
    expected_active_data_types.Put(syncer::POWER_BOOKMARK);
  }

  if (base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)) {
    expected_active_data_types.Put(syncer::WEBAUTHN_CREDENTIAL);
  }

  if (base::FeatureList::IsEnabled(syncer::kSyncAutofillWalletCredentialData)) {
    expected_active_data_types.Put(syncer::AUTOFILL_WALLET_CREDENTIAL);
  }

  if (base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
    expected_active_data_types.Put(syncer::PRODUCT_COMPARISON);
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Apps sync is controlled by a dedicated preference on Lacros,
  // corresponding to the Apps toggle in OS Sync settings. we remove
  // data types related to the Apps sync toggle.
  if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
    expected_active_data_types.RemoveAll(
        {syncer::APPS, syncer::APP_SETTINGS, syncer::WEB_APPS});
  }
#endif

  // The dictionary is currently only synced on Windows, Linux, and Lacros.
  // TODO(crbug.com/40118868): Reassess whether the following block needs to be
  // included in lacros-chrome once build flag switch of lacros-chrome is
  // complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Apps sync is controlled by a dedicated preference on Lacros,
  // corresponding to the Apps toggle in OS Sync settings.
  if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
    // Enable the Apps Toggle from OS level
    service->GetUserSettings()->SetAppsSyncEnabledByOs(true);
    // Wait until Sync has reconfigured itself and becomes active again.
    ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());
    expected_active_data_types.PutAll(
        {syncer::APPS, syncer::APP_SETTINGS, syncer::WEB_APPS});
    EXPECT_EQ(service->GetActiveDataTypes(), expected_active_data_types);
  }
#endif
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

}  // namespace
