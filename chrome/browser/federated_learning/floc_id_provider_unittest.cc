// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace federated_learning {

namespace {

using ComputeFlocTrigger = FlocIdProviderImpl::ComputeFlocTrigger;
using ComputeFlocResult = FlocIdProviderImpl::ComputeFlocResult;

class FakeFlocRemotePermissionService : public FlocRemotePermissionService {
 public:
  using FlocRemotePermissionService::FlocRemotePermissionService;

  void QueryFlocPermission(QueryFlocPermissionCallback callback,
                           const net::PartialNetworkTrafficAnnotationTag&
                               partial_traffic_annotation) override {
    std::move(callback).Run(swaa_nac_account_enabled_);
  }

  void set_swaa_nac_account_enabled(bool enabled) {
    swaa_nac_account_enabled_ = enabled;
  }

 private:
  bool swaa_nac_account_enabled_ = true;
};

class FakeCookieSettings : public content_settings::CookieSettings {
 public:
  using content_settings::CookieSettings::CookieSettings;

  void GetCookieSettingInternal(const GURL& url,
                                const GURL& first_party_url,
                                bool is_third_party_request,
                                content_settings::SettingSource* source,
                                ContentSetting* cookie_setting) const override {
    *cookie_setting =
        allow_cookies_internal_ ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  }

  bool ShouldBlockThirdPartyCookies() const override {
    return should_block_third_party_cookies_;
  }

  void set_should_block_third_party_cookies(
      bool should_block_third_party_cookies) {
    should_block_third_party_cookies_ = should_block_third_party_cookies;
  }

  void set_allow_cookies_internal(bool allow_cookies_internal) {
    allow_cookies_internal_ = allow_cookies_internal;
  }

 private:
  ~FakeCookieSettings() override = default;

  bool should_block_third_party_cookies_ = false;
  bool allow_cookies_internal_ = true;
};

class MockFlocIdProvider : public FlocIdProviderImpl {
 public:
  using FlocIdProviderImpl::FlocIdProviderImpl;

  void OnComputeFlocCompleted(ComputeFlocTrigger trigger,
                              ComputeFlocResult result) override {
    if (should_pause_before_compute_floc_completed_) {
      DCHECK(!paused_);
      paused_ = true;
      paused_trigger_ = trigger;
      paused_result_ = result;
      return;
    }

    ++compute_floc_completed_count_;
    FlocIdProviderImpl::OnComputeFlocCompleted(trigger, result);
  }

  void ContinueLastOnComputeFlocCompleted() {
    DCHECK(paused_);
    paused_ = false;
    ++compute_floc_completed_count_;
    FlocIdProviderImpl::OnComputeFlocCompleted(paused_trigger_, paused_result_);
  }

  void LogFlocComputedEvent(ComputeFlocTrigger trigger,
                            const ComputeFlocResult& result) override {
    ++log_event_count_;
    last_log_event_trigger_ = trigger;
    last_log_event_result_ = result;
    FlocIdProviderImpl::LogFlocComputedEvent(trigger, result);
  }

  size_t compute_floc_completed_count() const {
    return compute_floc_completed_count_;
  }

  void set_should_pause_before_compute_floc_completed(bool should_pause) {
    should_pause_before_compute_floc_completed_ = should_pause;
  }

  ComputeFlocResult paused_result() const {
    DCHECK(paused_);
    return paused_result_;
  }

  ComputeFlocTrigger paused_trigger() const {
    DCHECK(paused_);
    return paused_trigger_;
  }

  size_t log_event_count() const { return log_event_count_; }

  ComputeFlocTrigger last_log_event_trigger() const {
    DCHECK_LT(0u, log_event_count_);
    return last_log_event_trigger_;
  }

  ComputeFlocResult last_log_event_result() const {
    DCHECK_LT(0u, log_event_count_);
    return last_log_event_result_;
  }

 private:
  base::OnceCallback<void()> callback_before_compute_floc_completed_;

  // Add the support to be able to pause on the OnComputeFlocCompleted
  // execution and let it yield to other tasks posted to the same task runner.
  bool should_pause_before_compute_floc_completed_ = false;
  bool paused_ = false;
  ComputeFlocTrigger paused_trigger_;
  ComputeFlocResult paused_result_;

  size_t compute_floc_completed_count_ = 0u;
  size_t log_event_count_ = 0u;
  ComputeFlocTrigger last_log_event_trigger_;
  ComputeFlocResult last_log_event_result_;
};

}  // namespace

class FlocIdProviderUnitTest : public testing::Test {
 public:
  FlocIdProviderUnitTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~FlocIdProviderUnitTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false);

    TestingBrowserProcess::GetGlobal()->SetFlocBlocklistService(
        std::make_unique<FlocBlocklistService>());

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));

    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    test_sync_service_->SetTransportState(
        syncer::SyncService::TransportState::DISABLED);

    fake_user_event_service_ = std::make_unique<syncer::FakeUserEventService>();

    fake_floc_remote_permission_service_ =
        std::make_unique<FakeFlocRemotePermissionService>(
            /*url_loader_factory=*/nullptr);

    fake_cookie_settings_ = base::MakeRefCounted<FakeCookieSettings>(
        settings_map_.get(), &prefs_, false, "chrome-extension");

    floc_id_provider_ = std::make_unique<MockFlocIdProvider>(
        test_sync_service_.get(), fake_cookie_settings_,
        fake_floc_remote_permission_service_.get(), history_service_.get(),
        fake_user_event_service_.get());

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    settings_map_->ShutdownOnUIThread();
    history_service_->RemoveObserver(floc_id_provider_.get());
  }

  void CheckCanComputeFloc(
      FlocIdProviderImpl::CanComputeFlocCallback callback) {
    floc_id_provider_->CheckCanComputeFloc(std::move(callback));
  }

  void IsSwaaNacAccountEnabled(
      FlocIdProviderImpl::CanComputeFlocCallback callback) {
    floc_id_provider_->IsSwaaNacAccountEnabled(std::move(callback));
  }

  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) {
    floc_id_provider_->OnURLsDeleted(history_service, deletion_info);
  }

  void OnGetRecentlyVisitedURLsCompleted(ComputeFlocTrigger trigger,
                                         history::QueryResults results) {
    auto compute_floc_completed_callback =
        base::BindOnce(&FlocIdProviderImpl::OnComputeFlocCompleted,
                       base::Unretained(floc_id_provider_.get()), trigger);

    floc_id_provider_->OnGetRecentlyVisitedURLsCompleted(
        std::move(compute_floc_completed_callback), std::move(results));
  }

  void ExpireHistoryBefore(base::Time end_time) {
    base::CancelableTaskTracker tracker;
    base::RunLoop run_loop;
    history_service_->ExpireHistoryBeforeForTesting(
        end_time, run_loop.QuitClosure(), &tracker);
    run_loop.Run();
  }

  FlocId floc_id() const { return floc_id_provider_->floc_id_; }

  void set_floc_id(const FlocId& floc_id) const {
    floc_id_provider_->floc_id_ = floc_id;
  }

  bool floc_computation_in_progress() const {
    return floc_id_provider_->floc_computation_in_progress_;
  }

  void set_floc_computation_in_progress(bool floc_computation_in_progress) {
    floc_id_provider_->floc_computation_in_progress_ =
        floc_computation_in_progress;
  }

  bool first_floc_computation_triggered() const {
    return floc_id_provider_->first_floc_computation_triggered_;
  }

  void set_first_floc_computation_triggered(bool triggered) {
    floc_id_provider_->first_floc_computation_triggered_ = triggered;
  }

  void set_floc_id(const FlocId& floc_id) {
    floc_id_provider_->floc_id_ = floc_id;
  }

  base::Optional<ComputeFlocTrigger> pending_recompute_event() {
    return floc_id_provider_->pending_recompute_event_;
  }

  void SetRemoteSwaaNacAccountEnabled(bool enabled) {
    fake_floc_remote_permission_service_->set_swaa_nac_account_enabled(enabled);
  }

  void ForceScheduledUpdate() {
    floc_id_provider_->OnComputeFlocScheduledUpdate();
  }

  void OnBlocklistLoaded(const std::unordered_set<uint64_t>& blocklist) {
    g_browser_process->floc_blocklist_service()->OnBlocklistLoadResult(
        blocklist);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;

  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<syncer::FakeUserEventService> fake_user_event_service_;
  std::unique_ptr<FakeFlocRemotePermissionService>
      fake_floc_remote_permission_service_;
  scoped_refptr<FakeCookieSettings> fake_cookie_settings_;
  std::unique_ptr<MockFlocIdProvider> floc_id_provider_;

  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(FlocIdProviderUnitTest);
};

TEST_F(FlocIdProviderUnitTest, QualifiedInitialHistory) {
  // Add a history entry with a timestamp exactly 7 days back from now.
  std::string domain = "foo.com";

  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(7);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  task_environment_.RunUntilIdle();

  // Expect that the floc computation hasn't started, as the floc_id_provider
  // hasn't been notified about state of the sync_service.
  EXPECT_EQ(0u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(0u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_id().IsValid());
  EXPECT_FALSE(first_floc_computation_triggered());

  // Trigger the 1st floc computation.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_TRUE(floc_id().IsValid());
  EXPECT_EQ(FlocId::CreateFromHistory({domain}), floc_id());
  EXPECT_TRUE(first_floc_computation_triggered());

  // Advance the clock by 1 day. Expect a computation, as there's no history in
  // the last 7 days so the id has been reset to empty.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderUnitTest, UnqualifiedInitialHistory) {
  std::string domain = "foo.com";

  // Add a history entry with a timestamp 8 days back from now.
  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(8);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  task_environment_.RunUntilIdle();

  // Expect that the floc computation hasn't started, as the floc_id_provider
  // hasn't been notified about state of the sync_service.
  EXPECT_EQ(0u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(0u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_id().IsValid());
  EXPECT_FALSE(first_floc_computation_triggered());

  // Trigger the 1st floc computation.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_TRUE(first_floc_computation_triggered());

  // Add a history entry with a timestamp 6 days back from now.
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(6);
  history_service_->AddPage(add_page_args);

  // Advance the clock by 23 hours. Expect no more computation, as the id
  // refresh interval is 24 hours.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(23));

  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());

  // Advance the clock by 1 hour. Expect one more computation, as the refresh
  // time is reached and there's a valid history entry in the last 7 days.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_TRUE(floc_id().IsValid());
  EXPECT_EQ(FlocId::CreateFromHistory({domain}), floc_id());
}

TEST_F(FlocIdProviderUnitTest, HistoryDeleteAndScheduledUpdate) {
  std::string domain1 = "foo.com";
  std::string domain2 = "bar.com";

  // Add a history entry with a timestamp exactly 7 days back from now.
  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain1}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(7);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  // Add a history entry with a timestamp exactly 6 days back from now.
  add_page_args.url = GURL(base::StrCat({"https://www.", domain2}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(6);
  history_service_->AddPage(add_page_args);

  task_environment_.RunUntilIdle();

  // Trigger the 1st floc computation.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_TRUE(floc_id().IsValid());
  EXPECT_EQ(FlocId::CreateFromHistory({domain1, domain2}), floc_id());

  // Advance the clock by 12 hours. Expect no more computation.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(12));
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());

  // Expire the oldest history entry.
  ExpireHistoryBefore(base::Time::Now() - base::TimeDelta::FromDays(7));
  task_environment_.RunUntilIdle();

  // Expect one more computation due to the history deletion.
  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_TRUE(floc_id().IsValid());
  EXPECT_EQ(FlocId::CreateFromHistory({domain2}), floc_id());

  // Advance the clock by 23 hours. Expect no more computation, as the timer has
  // been reset due to the recomputation from history deletion.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(23));
  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());

  // Advance the clock by 1 hour. Expect one more computation, as the scheduled
  // time is reached. Expect an invalid floc id as there is no history in the
  // past 7 days.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_EQ(3u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(3u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderUnitTest, ScheduledUpdateSameFloc_NoNotification) {
  std::string domain = "foo.com";

  // Add a history entry with a timestamp 2 days back from now.
  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(2);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  task_environment_.RunUntilIdle();

  // Trigger the 1st floc computation.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId::CreateFromHistory({domain}), floc_id());

  // Advance the clock by 1 day. Expect one more computation, but the floc
  // didn't change.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId::CreateFromHistory({domain}), floc_id());
}

TEST_F(FlocIdProviderUnitTest, CheckCanComputeFloc_Success) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { EXPECT_TRUE(can_compute_floc); });

  CheckCanComputeFloc(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderUnitTest, CheckCanComputeFloc_Failure_SyncDisabled) {
  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { EXPECT_FALSE(can_compute_floc); });

  CheckCanComputeFloc(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderUnitTest,
       CheckCanComputeFloc_Failure_BlockThirdPartyCookies) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  fake_cookie_settings_->set_should_block_third_party_cookies(true);

  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { EXPECT_FALSE(can_compute_floc); });

  CheckCanComputeFloc(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderUnitTest,
       CheckCanComputeFloc_Failure_SwaaNacAccountDisabled) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  SetRemoteSwaaNacAccountEnabled(false);

  base::OnceCallback<void(bool)> cb = base::BindOnce(
      [](bool can_compute_floc) { EXPECT_FALSE(can_compute_floc); });

  CheckCanComputeFloc(std::move(cb));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderUnitTest, SwaaNacAccountEnabledUseCacheStatus) {
  base::OnceCallback<void(bool)> assert_enabled_callback_1 = base::BindOnce(
      [](bool can_compute_floc) { EXPECT_TRUE(can_compute_floc); });

  // The permission status in the fake_floc_remote_premission_service_ is by
  // default enabled.
  IsSwaaNacAccountEnabled(std::move(assert_enabled_callback_1));
  task_environment_.RunUntilIdle();

  // Turn off the permission in the fake_floc_remote_premission_service_.
  SetRemoteSwaaNacAccountEnabled(false);

  base::OnceCallback<void(bool)> assert_enabled_callback_2 = base::BindOnce(
      [](bool can_compute_floc) { EXPECT_TRUE(can_compute_floc); });

  // Fast forward by 11 hours. The cache is still valid.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(11));

  // The permission status is still enabled because it was obtained from the
  // cache.
  IsSwaaNacAccountEnabled(std::move(assert_enabled_callback_2));
  task_environment_.RunUntilIdle();

  // Fast forward by 1 hour so the cache becomes invalid.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  base::OnceCallback<void(bool)> assert_disabled_callback = base::BindOnce(
      [](bool can_compute_floc) { EXPECT_FALSE(can_compute_floc); });

  // The permission status should be obtained from the server again, and it's
  // now disabled.
  IsSwaaNacAccountEnabled(std::move(assert_disabled_callback));
  task_environment_.RunUntilIdle();
}

TEST_F(FlocIdProviderUnitTest, EventLogging) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFlocIdComputedEventLogging);

  // Event logging for browser start.
  floc_id_provider_->LogFlocComputedEvent(
      ComputeFlocTrigger::kBrowserStart,
      ComputeFlocResult(FlocId(12345ULL), FlocId(123ULL)));

  EXPECT_EQ(1u, fake_user_event_service_->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics1 =
      fake_user_event_service_->GetRecordedUserEvents()[0];
  EXPECT_EQ(specifics1.event_time_usec(),
            base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics1.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event1 =
      specifics1.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::NEW,
            event1.event_trigger());
  EXPECT_EQ(12345ULL, event1.floc_id());

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(3));

  // Event logging for scheduled update.
  floc_id_provider_->LogFlocComputedEvent(
      ComputeFlocTrigger::kScheduledUpdate,
      ComputeFlocResult(FlocId(999ULL), FlocId(777ULL)));

  EXPECT_EQ(2u, fake_user_event_service_->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics2 =
      fake_user_event_service_->GetRecordedUserEvents()[1];
  EXPECT_EQ(specifics2.event_time_usec(),
            base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics2.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event2 =
      specifics2.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::REFRESHED,
            event2.event_trigger());
  EXPECT_EQ(999ULL, event2.floc_id());

  // Event logging for invalid floc.
  floc_id_provider_->LogFlocComputedEvent(
      ComputeFlocTrigger::kScheduledUpdate,
      ComputeFlocResult(FlocId(), FlocId()));

  EXPECT_EQ(3u, fake_user_event_service_->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics3 =
      fake_user_event_service_->GetRecordedUserEvents()[2];
  EXPECT_EQ(specifics3.event_time_usec(),
            base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics3.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event3 =
      specifics3.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::REFRESHED,
            event3.event_trigger());
  EXPECT_FALSE(event3.has_floc_id());

  // Event logging for history delete.
  floc_id_provider_->LogFlocComputedEvent(
      ComputeFlocTrigger::kHistoryDelete,
      ComputeFlocResult(FlocId(555), FlocId(444)));

  EXPECT_EQ(4u, fake_user_event_service_->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics4 =
      fake_user_event_service_->GetRecordedUserEvents()[3];
  EXPECT_EQ(specifics4.event_time_usec(),
            base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics4.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event4 =
      specifics4.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::HISTORY_DELETE,
            event4.event_trigger());
  EXPECT_EQ(555ULL, event4.floc_id());

  // Event logging for blocked floc.
  floc_id_provider_->LogFlocComputedEvent(
      ComputeFlocTrigger::kScheduledUpdate,
      ComputeFlocResult(FlocId(87654), FlocId(45678)));

  EXPECT_EQ(5u, fake_user_event_service_->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics5 =
      fake_user_event_service_->GetRecordedUserEvents()[4];
  EXPECT_EQ(specifics5.event_time_usec(),
            base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics5.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event5 =
      specifics5.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::REFRESHED,
            event5.event_trigger());
  EXPECT_EQ(87654ULL, event5.floc_id());
}

TEST_F(FlocIdProviderUnitTest, HistoryDelete_AllHistory) {
  base::Time time = base::Time::Now() - base::TimeDelta::FromDays(9);

  history::URLResult url_result(GURL("https://a.test"), time);
  url_result.set_publicly_routable(true);

  history::QueryResults query_results;
  query_results.SetURLResults({url_result});

  set_first_floc_computation_triggered(true);
  set_floc_computation_in_progress(true);

  OnGetRecentlyVisitedURLsCompleted(ComputeFlocTrigger::kBrowserStart,
                                    std::move(query_results));
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_id().IsValid());

  OnURLsDeleted(history_service_.get(), history::DeletionInfo::ForAllHistory());
  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderUnitTest, HistoryDelete_InvalidTimeRange) {
  base::Time time = base::Time::Now() - base::TimeDelta::FromDays(9);

  GURL url_a = GURL("https://a.test");

  history::URLResult url_result(url_a, time);
  url_result.set_publicly_routable(true);

  history::QueryResults query_results;
  query_results.SetURLResults({url_result});

  set_first_floc_computation_triggered(true);
  set_floc_computation_in_progress(true);

  OnGetRecentlyVisitedURLsCompleted(ComputeFlocTrigger::kBrowserStart,
                                    std::move(query_results));
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_id().IsValid());

  OnURLsDeleted(history_service_.get(),
                history::DeletionInfo::ForUrls(
                    {history::URLResult(url_a, base::Time())}, {}));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderUnitTest, HistoryDelete_TimeRange) {
  base::Time time = base::Time::Now() - base::TimeDelta::FromDays(9);

  history::URLResult url_result(GURL("https://a.test"), time);
  url_result.set_publicly_routable(true);

  history::QueryResults query_results;
  query_results.SetURLResults({url_result});

  set_first_floc_computation_triggered(true);
  set_floc_computation_in_progress(true);

  OnGetRecentlyVisitedURLsCompleted(ComputeFlocTrigger::kBrowserStart,
                                    std::move(query_results));
  EXPECT_FALSE(floc_computation_in_progress());
  EXPECT_TRUE(floc_id().IsValid());

  history::DeletionInfo deletion_info(history::DeletionTimeRange(time, time),
                                      false, {}, {},
                                      base::Optional<std::set<GURL>>());

  OnURLsDeleted(history_service_.get(), deletion_info);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderUnitTest, HistoryEntriesWithPrivateIP) {
  history::QueryResults query_results;
  query_results.SetURLResults(
      {history::URLResult(GURL("https://a.test"),
                          base::Time::Now() - base::TimeDelta::FromDays(1))});

  set_first_floc_computation_triggered(true);
  set_floc_computation_in_progress(true);

  OnGetRecentlyVisitedURLsCompleted(ComputeFlocTrigger::kBrowserStart,
                                    std::move(query_results));

  EXPECT_FALSE(floc_id().IsValid());
}

TEST_F(FlocIdProviderUnitTest, MultipleHistoryEntries) {
  base::Time time = base::Time::Now() - base::TimeDelta::FromDays(1);

  history::URLResult url_result_a(GURL("https://a.test"), time);
  url_result_a.set_publicly_routable(true);

  history::URLResult url_result_b(GURL("https://b.test"), time);
  url_result_b.set_publicly_routable(true);

  history::URLResult url_result_c(GURL("https://c.test"), time);

  std::vector<history::URLResult> url_results{url_result_a, url_result_b,
                                              url_result_c};

  history::QueryResults query_results;
  query_results.SetURLResults(std::move(url_results));

  set_first_floc_computation_triggered(true);
  set_floc_computation_in_progress(true);

  OnGetRecentlyVisitedURLsCompleted(ComputeFlocTrigger::kBrowserStart,
                                    std::move(query_results));

  EXPECT_EQ(FlocId::CreateFromHistory({"a.test", "b.test"}), floc_id());
}

TEST_F(
    FlocIdProviderUnitTest,
    DISABLED_BlocklistFilteringEnabled_SyncHistoryEnabledFollowedByBlocklistLoaded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFlocIdBlocklistFiltering);

  // Turn on sync & sync-history. The 1st floc computation should not be
  // triggered as the blocklist hasn't been loaded yet.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  EXPECT_FALSE(first_floc_computation_triggered());

  // Load the blocklist. The 1st floc computation should be triggered now as
  // sync & sync-history are enabled the blocklist is loaded.
  OnBlocklistLoaded({});

  EXPECT_TRUE(first_floc_computation_triggered());
}

TEST_F(
    FlocIdProviderUnitTest,
    DISABLED_BlocklistFilteringEnabled_BlocklistLoadedFollowedBySyncHistoryEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kFlocIdBlocklistFiltering);

  // Load the blocklist. The 1st floc computation should not be
  // triggered as sync & sync-history are not enabled yet.
  OnBlocklistLoaded({});

  EXPECT_FALSE(first_floc_computation_triggered());

  // Turn on sync & sync-history. The 1st floc computation should be triggered
  // now as sync & sync-history are enabled the blocklist is loaded.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  EXPECT_TRUE(first_floc_computation_triggered());
}

TEST_F(FlocIdProviderUnitTest, BlocklistFilteringEnabled_BlockedFloc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kFlocIdComputedEventLogging,
                                 features::kFlocIdBlocklistFiltering},
                                {});

  std::string domain = "foo.com";

  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(1);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  task_environment_.RunUntilIdle();

  // Load the blocklist and turn on sync & sync-history to trigger the 1st floc
  // computation.
  std::unordered_set<uint64_t> blocklist;
  OnBlocklistLoaded(blocklist);

  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  EXPECT_TRUE(first_floc_computation_triggered());

  task_environment_.RunUntilIdle();

  FlocId floc_from_history = FlocId::CreateFromHistory({domain});

  // Expect a computation. The floc should be equal to the sim-hash of the
  // history.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_EQ(floc_from_history, floc_id());

  // Insert the current floc to blocklist and reload it.
  blocklist.insert(floc_from_history.ToUint64());
  OnBlocklistLoaded(blocklist);

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  // Expect one more computation, where the result contains a valid sim_hash and
  // an invalid final_hash, as it was blocked. The internal floc is set to the
  // invalid one.
  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_EQ(floc_id_provider_->last_log_event_result().sim_hash,
            floc_from_history);
  EXPECT_FALSE(floc_id_provider_->last_log_event_result().final_hash.IsValid());
  EXPECT_FALSE(floc_id().IsValid());

  // In the event when the sim_hash is valid and final_hash is invalid, we'll
  // still log it.
  EXPECT_EQ(2u, fake_user_event_service_->GetRecordedUserEvents().size());
  const sync_pb::UserEventSpecifics& specifics =
      fake_user_event_service_->GetRecordedUserEvents()[1];
  EXPECT_EQ(specifics.event_time_usec(),
            base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
            specifics.event_case());

  const sync_pb::UserEventSpecifics_FlocIdComputed& event =
      specifics.floc_id_computed_event();
  EXPECT_EQ(sync_pb::UserEventSpecifics::FlocIdComputed::REFRESHED,
            event.event_trigger());
  EXPECT_EQ(floc_from_history.ToUint64(), event.floc_id());

  // Reset and reload the blocklist.
  blocklist.clear();
  OnBlocklistLoaded(blocklist);

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  // Expect one more computation. The floc should be equal to the sim-hash of
  // the history.
  EXPECT_EQ(3u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(3u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId::CreateFromHistory({domain}), floc_id());
}

TEST_F(FlocIdProviderUnitTest, TurnSyncOffAndOn) {
  std::string domain = "foo.com";

  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(1);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  task_environment_.RunUntilIdle();

  // Trigger the 1st floc computation.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId::CreateFromHistory({domain}), floc_id());

  // Turn off sync.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::DISABLED);

  // Advance the clock by 1 day. Expect one more computation, as the sync was
  // turned off so the id has been reset to empty.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_EQ(2u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_FALSE(floc_id().IsValid());

  // Turn on sync.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  // Advance the clock by 1 day. Expect one more floc computation.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_EQ(3u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(3u, floc_id_provider_->log_event_count());
  EXPECT_EQ(FlocId::CreateFromHistory({domain}), floc_id());
}

TEST_F(FlocIdProviderUnitTest, GetInterestCohortForJsApiMethod) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  set_floc_id(FlocId(123));

  EXPECT_EQ(FlocId(123).ToString(),
            floc_id_provider_->GetInterestCohortForJsApi(
                /*requesting_origin=*/{}, /*site_for_cookies=*/{}));
}

TEST_F(FlocIdProviderUnitTest,
       GetInterestCohortForJsApiMethod_SyncHistoryDisabled) {
  set_floc_id(FlocId(123));
  EXPECT_EQ(std::string(),
            floc_id_provider_->GetInterestCohortForJsApi(
                /*requesting_origin=*/{}, /*site_for_cookies=*/{}));
}

TEST_F(FlocIdProviderUnitTest,
       GetInterestCohortForJsApiMethod_ThirdPartyCookiesDisabled) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  set_floc_id(FlocId(123));

  fake_cookie_settings_->set_should_block_third_party_cookies(true);

  EXPECT_EQ(std::string(),
            floc_id_provider_->GetInterestCohortForJsApi(
                /*requesting_origin=*/{}, /*site_for_cookies=*/{}));
}

TEST_F(FlocIdProviderUnitTest,
       GetInterestCohortForJsApiMethod_CookiesContentSettingsDisallowed) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  set_floc_id(FlocId(123));

  fake_cookie_settings_->set_allow_cookies_internal(false);

  EXPECT_EQ(std::string(),
            floc_id_provider_->GetInterestCohortForJsApi(
                /*requesting_origin=*/{}, /*site_for_cookies=*/{}));
}

TEST_F(FlocIdProviderUnitTest,
       GetInterestCohortForJsApiMethod_FlocUnavailable) {
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  EXPECT_EQ(std::string(),
            floc_id_provider_->GetInterestCohortForJsApi(
                /*requesting_origin=*/{}, /*site_for_cookies=*/{}));
}

TEST_F(FlocIdProviderUnitTest, HistoryDeleteDuringInProgressComputation) {
  std::string domain1 = "foo.com";
  std::string domain2 = "bar.com";
  std::string domain3 = "baz.com";

  // Add a history entry with a timestamp exactly 7 days back from now.
  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain1}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(7);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  // Add a history entry with a timestamp exactly 6 days back from now.
  add_page_args.url = GURL(base::StrCat({"https://www.", domain2}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(6);
  history_service_->AddPage(add_page_args);

  // Add a history entry with a timestamp exactly 5 days back from now.
  add_page_args.url = GURL(base::StrCat({"https://www.", domain3}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(5);
  history_service_->AddPage(add_page_args);

  // Trigger the 1st floc computation.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_TRUE(floc_id().IsValid());
  EXPECT_EQ(FlocId::CreateFromHistory({domain1, domain2, domain3}), floc_id());

  // Advance the clock by 1 day. The "domain1" should expire. However, we pause
  // before the computation completes.
  floc_id_provider_->set_should_pause_before_compute_floc_completed(true);
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(pending_recompute_event().has_value());
  EXPECT_EQ(FlocId::CreateFromHistory({domain1, domain2, domain3}), floc_id());
  EXPECT_EQ(FlocId::CreateFromHistory({domain2, domain3}),
            floc_id_provider_->paused_result().final_hash);
  EXPECT_EQ(ComputeFlocTrigger::kScheduledUpdate,
            floc_id_provider_->paused_trigger());

  // Expire the "domain2" history entry right before the floc computation
  // completes. Since the computation is still considered to be in-progress, a
  // new recompute event due to this delete will be scheduled to happen right
  // after this computation completes.
  ExpireHistoryBefore(base::Time::Now() - base::TimeDelta::FromDays(7));

  EXPECT_TRUE(pending_recompute_event().has_value());
  EXPECT_EQ(ComputeFlocTrigger::kHistoryDelete,
            pending_recompute_event().value());

  floc_id_provider_->set_should_pause_before_compute_floc_completed(false);
  floc_id_provider_->ContinueLastOnComputeFlocCompleted();
  task_environment_.RunUntilIdle();

  // Expect 2 more compute completion events and 1 more log event. This is
  // because we won't send log event if there's a recompute event scheduled.
  EXPECT_EQ(3u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(2u, floc_id_provider_->log_event_count());
  EXPECT_EQ(ComputeFlocTrigger::kHistoryDelete,
            floc_id_provider_->last_log_event_trigger());
  EXPECT_FALSE(pending_recompute_event().has_value());

  // The final floc should be derived from "domain3".
  EXPECT_TRUE(floc_id().IsValid());
  EXPECT_EQ(FlocId::CreateFromHistory({domain3}), floc_id());
}

TEST_F(FlocIdProviderUnitTest, ScheduledUpdateDuringInProgressComputation) {
  std::string domain1 = "foo.com";
  std::string domain2 = "bar.com";
  std::string domain3 = "baz.com";

  // Add a history entry with a timestamp exactly 7 days back from now.
  history::HistoryAddPageArgs add_page_args;
  add_page_args.url = GURL(base::StrCat({"https://www.", domain1}));
  add_page_args.time = base::Time::Now() - base::TimeDelta::FromDays(7);
  add_page_args.publicly_routable = true;
  history_service_->AddPage(add_page_args);

  // Trigger the 1st floc computation.
  test_sync_service_->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_->FireStateChanged();

  EXPECT_TRUE(floc_computation_in_progress());
  EXPECT_FALSE(pending_recompute_event().has_value());

  // Scheduled update during an in-progress computation won't set the pending
  // event.
  ForceScheduledUpdate();
  EXPECT_FALSE(pending_recompute_event().has_value());

  task_environment_.RunUntilIdle();

  // Expect that the 1st computation has completed.
  EXPECT_EQ(1u, floc_id_provider_->compute_floc_completed_count());
  EXPECT_EQ(1u, floc_id_provider_->log_event_count());
  EXPECT_TRUE(floc_id().IsValid());
  EXPECT_EQ(FlocId::CreateFromHistory({domain1}), floc_id());
}

}  // namespace federated_learning
