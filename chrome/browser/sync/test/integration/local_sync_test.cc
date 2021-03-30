// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/test/browser_test.h"
#include "crypto/ec_private_key.h"

namespace {

using syncer::ProfileSyncService;

class SyncTransportActiveChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncTransportActiveChecker(ProfileSyncService* service)
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
 protected:
  LocalSyncTest() {
    EXPECT_TRUE(local_sync_backend_dir_.CreateUniqueTempDir());
  }

  ~LocalSyncTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // By default on Window OS local sync backend uses roaming profile. It can
    // lead to problems if some tests run simultaneously and use the same
    // roaming profile.
    auto file = local_sync_backend_dir_.GetPath().Append(
        FILE_PATH_LITERAL("profile.pb"));
    command_line->AppendSwitchASCII(switches::kLocalSyncBackendDir,
                                    file.MaybeAsASCII());
    command_line->AppendSwitch(switches::kEnableLocalSyncBackend);
    command_line->AppendSwitchASCII(
        switches::kSyncDeferredStartupTimeoutSeconds, "1");
  }

 private:
  base::ScopedTempDir local_sync_backend_dir_;
  DISALLOW_COPY_AND_ASSIGN(LocalSyncTest);
};

// The local sync backend is currently only supported on Windows, Mac and Linux.
// TODO(crbug.com/1052397): Reassess whether the following block needs to be
// included in lacros-chrome once build flag switch of lacros-chrome is
// complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
IN_PROC_BROWSER_TEST_F(LocalSyncTest, ShouldStart) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
          browser()->profile());

  // Wait until the first sync cycle is completed.
  ASSERT_TRUE(SyncTransportActiveChecker(service).Wait());

  EXPECT_TRUE(service->IsLocalSyncEnabled());
  EXPECT_FALSE(service->IsSyncFeatureEnabled());
  EXPECT_FALSE(service->IsSyncFeatureActive());

  // Verify that the expected set of data types successfully started up.
  // If this test fails after adding a new data type, carefully consider whether
  // the type should be enabled in Local Sync mode, i.e. for roaming profiles on
  // Windows.
  // TODO(crbug.com/1109640): Consider whether all of these types should really
  // be enabled in Local Sync mode.
  syncer::ModelTypeSet expected_active_data_types = syncer::ModelTypeSet(
      syncer::BOOKMARKS, syncer::PREFERENCES, syncer::PASSWORDS,
      syncer::AUTOFILL_PROFILE, syncer::AUTOFILL, syncer::AUTOFILL_WALLET_DATA,
      syncer::AUTOFILL_WALLET_METADATA, syncer::THEMES, syncer::TYPED_URLS,
      syncer::EXTENSIONS, syncer::SEARCH_ENGINES, syncer::SESSIONS,
      syncer::APPS, syncer::APP_SETTINGS, syncer::EXTENSION_SETTINGS,
      syncer::HISTORY_DELETE_DIRECTIVES, syncer::DEVICE_INFO,
      syncer::PRIORITY_PREFERENCES, syncer::WEB_APPS, syncer::PROXY_TABS,
      syncer::NIGORI);

  // The dictionary is currently only synced on Windows and Linux.
  // TODO(crbug.com/1052397): Reassess whether the following block needs to be
  // included
  // in lacros-chrome once build flag switch of lacros-chrome is
  // complete.

#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  expected_active_data_types.Put(syncer::DICTIONARY);
#endif

  if (base::FeatureList::IsEnabled(reading_list::switches::kReadLater)) {
    expected_active_data_types.Put(syncer::READING_LIST);
  }

  EXPECT_EQ(service->GetActiveDataTypes(), expected_active_data_types);

  // Verify certain features are disabled.
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::USER_CONSENTS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::USER_EVENTS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SECURITY_EVENTS));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SEND_TAB_TO_SELF));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE));
  EXPECT_FALSE(send_tab_to_self::IsUserSyncTypeActive(browser()->profile()));
}
#endif  // defined(OS_WIN) || defined(OS_MAC) || (defined(OS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

}  // namespace
