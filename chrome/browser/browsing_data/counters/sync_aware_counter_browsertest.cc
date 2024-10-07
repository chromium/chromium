// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"

using browsing_data::BrowsingDataCounter;

namespace {

static const int kFirstProfileIndex = 0;

// A test for the sync behavior of several BrowsingDataCounters.
class SyncAwareCounterTest : public SyncTest {
 public:
  SyncAwareCounterTest() : SyncTest(SINGLE_CLIENT) {}

  SyncAwareCounterTest(const SyncAwareCounterTest&) = delete;
  SyncAwareCounterTest& operator=(const SyncAwareCounterTest&) = delete;

  ~SyncAwareCounterTest() override {}

  void SetUpOnMainThread() override {
    fake_web_history_service_ =
        std::make_unique<history::FakeWebHistoryService>();
    run_loop_ = std::make_unique<base::RunLoop>();
    SyncTest::SetUpOnMainThread();
  }

  history::WebHistoryService* GetFakeWebHistoryService(Profile* profile) {
    // Check if history sync is enabled.
    if (WebHistoryServiceFactory::GetForProfile(profile)) {
      return fake_web_history_service_.get();
    }
    return nullptr;
  }

  // Callback and result retrieval ---------------------------------------------

  void WaitForCounting() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
    finished_ = false;
  }

  // Returns true if the counter finished since the last call to
  // WaitForCounting() or CountingFinishedSinceLastAsked()
  bool CountingFinishedSinceLastAsked() {
    bool result = finished_;
    finished_ = false;
    return result;
  }

  bool IsSyncEnabled() { return sync_enabled_; }

  void OnCounterResult(std::unique_ptr<BrowsingDataCounter::Result> result) {
    finished_ = result->Finished();

    if (finished_) {
      auto* sync_result =
          static_cast<BrowsingDataCounter::SyncResult*>(result.get());
      sync_enabled_ = sync_result->is_sync_enabled();
      run_loop_->Quit();
    }
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<history::FakeWebHistoryService> fake_web_history_service_;

  bool finished_;
  bool sync_enabled_;
};

// Test that the counting restarts when autofill sync state changes.
// TODO(crbug.com/40443942): Move this to the sync/test/integration directory?
IN_PROC_BROWSER_TEST_F(SyncAwareCounterTest, AutofillCounter) {
  // Set up the Sync client.
  ASSERT_TRUE(SetupClients());
  syncer::SyncService* sync_service = GetSyncService(kFirstProfileIndex);
  Profile* profile = GetProfile(kFirstProfileIndex);
  // Set up the counter.
  browsing_data::AutofillCounter counter(
      autofill::PersonalDataManagerFactory::GetForBrowserContext(profile),
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS),
      /*user_annotations_service=*/nullptr, sync_service);

  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&SyncAwareCounterTest::OnCounterResult,
                                   base::Unretained(this)));

  // We sync all datatypes by default, so starting Sync means that we start
  // syncing autofill, and this should restart the counter.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_service->IsSyncFeatureActive());
  ASSERT_TRUE(sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL));
  WaitForCounting();
  EXPECT_TRUE(IsSyncEnabled());

  // We stop syncing autofill in particular. This restarts the counter.
  syncer::UserSelectableTypeSet everything_except_autofill =
      GetRegisteredSelectableTypes(kFirstProfileIndex);
  everything_except_autofill.Remove(syncer::UserSelectableType::kAutofill);
  auto sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, everything_except_autofill);
  ASSERT_FALSE(sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
  sync_blocker.reset();
  WaitForCounting();
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL));
  EXPECT_FALSE(IsSyncEnabled());

  // If autofill sync is not affected, the counter is not restarted.
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kHistory});
  sync_blocker.reset();
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // We start syncing autofill again. This restarts the counter.
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      GetRegisteredSelectableTypes(kFirstProfileIndex));
  sync_blocker.reset();
  WaitForCounting();
  EXPECT_TRUE(IsSyncEnabled());

  // Signout isn't possible on ChromeOS (Ash).
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Stopping the Sync service triggers a restart.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForCounting();
  EXPECT_FALSE(IsSyncEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

// Test that the counting restarts when password sync state changes.
// TODO(crbug.com/40443942): Move this to the sync/test/integration directory?
IN_PROC_BROWSER_TEST_F(SyncAwareCounterTest, PasswordCounter) {
  // Set up the Sync client.
  ASSERT_TRUE(SetupClients());
  syncer::SyncService* sync_service = GetSyncService(kFirstProfileIndex);
  Profile* profile = GetProfile(kFirstProfileIndex);
  // Set up the counter.
  browsing_data::PasswordsCounter counter(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPrefs(), sync_service);

  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&SyncAwareCounterTest::OnCounterResult,
                                   base::Unretained(this)));

  // We sync all datatypes by default, so starting Sync means that we start
  // syncing passwords, and this should restart the counter.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_service->IsSyncFeatureActive());
  ASSERT_TRUE(sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPasswords));
  WaitForCounting();
  EXPECT_TRUE(IsSyncEnabled());

  // We stop syncing passwords in particular. This restarts the counter.
  syncer::UserSelectableTypeSet everything_except_passwords =
      GetRegisteredSelectableTypes(kFirstProfileIndex);
  everything_except_passwords.Remove(syncer::UserSelectableType::kPasswords);
  auto sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, everything_except_passwords);
  ASSERT_FALSE(sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPasswords));
  sync_blocker.reset();
  WaitForCounting();
  ASSERT_FALSE(sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPasswords));
  EXPECT_FALSE(IsSyncEnabled());

  // If password sync is not affected, the counter is not restarted.
  syncer::UserSelectableTypeSet only_history = {
      syncer::UserSelectableType::kHistory};
  sync_service->GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                    only_history);
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                    only_history);
  sync_blocker.reset();
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // We start syncing passwords again. This restarts the counter.
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      GetRegisteredSelectableTypes(kFirstProfileIndex));
  sync_blocker.reset();
  WaitForCounting();
  EXPECT_TRUE(IsSyncEnabled());

  // Signout isn't possible on ChromeOS (Ash).
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Stopping the Sync service triggers a restart.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForCounting();
  EXPECT_FALSE(IsSyncEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

// Test that the counting restarts when history sync state changes.
// TODO(crbug.com/40443942): Move this to the sync/test/integration directory?
IN_PROC_BROWSER_TEST_F(SyncAwareCounterTest, HistoryCounter) {
  // Set up the Sync client.
  ASSERT_TRUE(SetupClients());
  syncer::SyncService* sync_service = GetSyncService(kFirstProfileIndex);
  Profile* profile = GetProfile(kFirstProfileIndex);

  // Set up the fake web history service and the counter.

  browsing_data::HistoryCounter counter(
      HistoryServiceFactory::GetForProfileWithoutCreating(browser()->profile()),
      base::BindRepeating(&SyncAwareCounterTest::GetFakeWebHistoryService,
                          base::Unretained(this), base::Unretained(profile)),
      sync_service);

  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&SyncAwareCounterTest::OnCounterResult,
                                   base::Unretained(this)));

  // We sync all datatypes by default, so starting Sync means that we start
  // syncing history deletion, and this should restart the counter.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_service->IsSyncFeatureActive());
  ASSERT_TRUE(sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  ASSERT_TRUE(sync_service->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));

  WaitForCounting();
  EXPECT_TRUE(IsSyncEnabled());

  // We stop syncing history deletion in particular. This restarts the counter.
  syncer::UserSelectableTypeSet everything_except_history =
      GetRegisteredSelectableTypes(kFirstProfileIndex);
  everything_except_history.Remove(syncer::UserSelectableType::kHistory);
  auto sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, everything_except_history);
  sync_blocker.reset();
  WaitForCounting();
  EXPECT_FALSE(IsSyncEnabled());

  // If the history deletion sync is not affected, the counter is not restarted.
  syncer::UserSelectableTypeSet only_passwords = {
      syncer::UserSelectableType::kPasswords};
  sync_service->GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                    only_passwords);
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                    only_passwords);
  sync_blocker.reset();
  EXPECT_FALSE(counter.HasTrackedTasksForTesting());
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // Same in this case.
  syncer::UserSelectableTypeSet autofill_and_passwords = {
      syncer::UserSelectableType::kAutofill,
      syncer::UserSelectableType::kPasswords};
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, autofill_and_passwords);
  sync_blocker.reset();
  EXPECT_FALSE(counter.HasTrackedTasksForTesting());
  EXPECT_FALSE(CountingFinishedSinceLastAsked());

  // We start syncing history deletion again. This restarts the counter.
  sync_blocker = sync_service->GetSetupInProgressHandle();
  sync_service->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      GetRegisteredSelectableTypes(kFirstProfileIndex));
  sync_blocker.reset();
  WaitForCounting();
  EXPECT_TRUE(IsSyncEnabled());

  // Changing the syncing datatypes to another set that still includes history
  // deletion should technically not trigger a restart, because the state of
  // history deletion did not change. However, in reality we can get two
  // notifications, one that history sync has stopped and another that it is
  // active again.

  // Signout isn't possible on ChromeOS (Ash).
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Stopping the Sync service triggers a restart.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForCounting();
  EXPECT_FALSE(IsSyncEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace
