// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/privacy_metrics_service.h"

#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kClearOnExitSyncEventHistogram[] = "Privacy.ClearOnExitSyncEvent";

}

class PrivacyMetricsServiceTest : public testing::Test {
 protected:
  void CreateService(bool clear_on_exit_enabled,
                     signin::ConsentLevel consent_level) {
    SetClearOnExitEnabled(clear_on_exit_enabled);
    SetPrimaryAccountConsentLevel(consent_level);
    sync_service()->SetMaxTransportState(
        syncer::SyncService::TransportState::INITIALIZING);

    privacy_metrics_service_ = std::make_unique<PrivacyMetricsService>(
        profile()->GetPrefs(),
        HostContentSettingsMapFactory::GetForProfile(profile()), sync_service(),
        identity_test_env()->identity_manager());
  }

  void SetClearOnExitEnabled(bool enabled) {
    auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
    map->SetDefaultContentSetting(
        ContentSettingsType::COOKIES,
        enabled ? ContentSetting::CONTENT_SETTING_SESSION_ONLY
                : ContentSetting::CONTENT_SETTING_ALLOW);
  }

  void ActivateSync() {
    SetPrimaryAccountConsentLevel(signin::ConsentLevel::kSync);
    sync_service()->ClearAuthError();
    sync_service()->SetMaxTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    ASSERT_EQ(sync_service()->GetTransportState(),
              syncer::SyncService::TransportState::ACTIVE);
    sync_service()->FireStateChanged();
  }

  void PauseSync() {
    SetPrimaryAccountConsentLevel(signin::ConsentLevel::kSync);
    sync_service()->SetPersistentAuthError();
    ASSERT_EQ(sync_service()->GetTransportState(),
              syncer::SyncService::TransportState::PAUSED);
    sync_service()->FireStateChanged();
  }

  void DisableSync() {
    identity_test_env_.ClearPrimaryAccount();
    sync_service()->SetSignedOut();
    ASSERT_EQ(sync_service()->GetTransportState(),
              syncer::SyncService::TransportState::DISABLED);
    sync_service()->FireStateChanged();
  }

  void SetPrimaryAccountConsentLevel(signin::ConsentLevel consent_level) {
    if (consent_level == signin::ConsentLevel::kSignin &&
        identity_test_env()->identity_manager()->HasPrimaryAccount(
            signin::ConsentLevel::kSync)) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      identity_test_env()->RevokeSyncConsent();
#else
      NOTREACHED_IN_MIGRATION()
          << "It is not possible to unconsent from Sync on Ash";
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
      return;
    }

    CoreAccountInfo account_info =
        identity_test_env()->identity_manager()->GetPrimaryAccountInfo(
            consent_level);
    if (account_info.IsEmpty()) {
      account_info = identity_test_env()->SetPrimaryAccount("test@test.com",
                                                            consent_level);
    }

    sync_service_.SetSignedIn(consent_level, account_info);
  }

  TestingProfile* profile() { return &profile_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  syncer::TestSyncService* sync_service() { return &sync_service_; }

  PrivacyMetricsService* privacy_metrics_service() {
    return privacy_metrics_service_.get();
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  TestingProfile profile_;
  syncer::TestSyncService sync_service_;

  std::unique_ptr<PrivacyMetricsService> privacy_metrics_service_;
};

TEST_F(PrivacyMetricsServiceTest, BasicShutdownMetrics) {
  // Check that metrics recorded on profile shutdown, which do not depend on
  // events during the profile lifetime, are correct.
  CreateService(/*clear_on_exit_enabled=*/true, signin::ConsentLevel::kSync);
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kClearOnExitSyncEventHistogram, 0);

  // If a profile is not syncing, or does not have COE enabled, no shutdown
  // metrics should be recorded.
  SetClearOnExitEnabled(false);
  ActivateSync();
  privacy_metrics_service()->Shutdown();
  histogram_tester.ExpectTotalCount(kClearOnExitSyncEventHistogram, 0);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Disabling Sync is not possible on Ash.
  SetClearOnExitEnabled(true);
  DisableSync();
  privacy_metrics_service()->Shutdown();
  histogram_tester.ExpectTotalCount(kClearOnExitSyncEventHistogram, 0);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // If COE is enabled, and sync is paused, an event should be recorded.
  SetClearOnExitEnabled(true);
  PauseSync();
  privacy_metrics_service()->Shutdown();
  histogram_tester.ExpectUniqueSample(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::kShutdownSyncPaused, 1);
}

TEST_F(PrivacyMetricsServiceTest, FixSyncPausedThroughReLogin) {
  // Confirm that when the user fixes the sync paused state by logging back
  // into the same account, the appropriate events are recorded.
  CreateService(/*clear_on_exit_enabled=*/true, signin::ConsentLevel::kSync);
  base::HistogramTester histogram_tester;
  PauseSync();
  histogram_tester.ExpectUniqueSample(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::kStartupSyncPaused, 1);

  ActivateSync();
  histogram_tester.ExpectBucketCount(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::kReloginToPausedAccount, 1);

  privacy_metrics_service()->Shutdown();
  histogram_tester.ExpectBucketCount(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::
          kShutdownSyncActiveStartedPausedNoConsentChange,
      1);
  histogram_tester.ExpectTotalCount(kClearOnExitSyncEventHistogram, 3);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Confirm that when the user fixes the sync paused state by logging out of
// the affected account, the appropriate events are recorded.
// Logging out of the Sync account is not possible on Ash.
TEST_F(PrivacyMetricsServiceTest, FixSyncPausedThroughLogout) {
  CreateService(/*clear_on_exit_enabled=*/true, signin::ConsentLevel::kSync);
  base::HistogramTester histogram_tester;
  PauseSync();
  histogram_tester.ExpectUniqueSample(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::kStartupSyncPaused, 1);

  DisableSync();
  histogram_tester.ExpectBucketCount(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::kLogoutOfPausedAccount, 1);

  ActivateSync();
  privacy_metrics_service()->Shutdown();
  histogram_tester.ExpectBucketCount(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::
          kShutdownSyncActiveStartedPausedConsentChange,
      1);
  histogram_tester.ExpectTotalCount(kClearOnExitSyncEventHistogram, 3);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PrivacyMetricsServiceTest, NoSyncIssues) {
  // Check that if a user has Clear on Exit enabled, but does not experience
  // any relevant sync paused issues, the correct events are recorded.
  base::HistogramTester histogram_tester;
  CreateService(/*clear_on_exit_enabled=*/true, signin::ConsentLevel::kSync);
  ActivateSync();

  histogram_tester.ExpectUniqueSample(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::kStartupSyncActive, 1);

  privacy_metrics_service()->Shutdown();
  histogram_tester.ExpectBucketCount(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::
          kShutdownSyncActiveStartedActiveNoConsentChange,
      1);
  histogram_tester.ExpectTotalCount(kClearOnExitSyncEventHistogram, 2);
}

TEST_F(PrivacyMetricsServiceTest, AccountChangeNoSyncIssues) {
  // Check that if the profile is shutting down with sync & COE enabled, but
  // didn't start paused, and the account consent changed, the correct event
  // is recorded.
  CreateService(/*clear_on_exit_enabled=*/true, signin::ConsentLevel::kSignin);
  base::HistogramTester histogram_tester;

  ActivateSync();
  privacy_metrics_service()->Shutdown();
  histogram_tester.ExpectUniqueSample(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::
          kShutdownSyncActiveStartedActiveConsentChange,
      1);
}

TEST_F(PrivacyMetricsServiceTest, StartupNoSync) {
  // Check if the user has COE enabled, but not sync, the appropriate event is
  // recorded.
  base::HistogramTester histogram_tester;
  CreateService(/*clear_on_exit_enabled=*/true, signin::ConsentLevel::kSignin);
  histogram_tester.ExpectUniqueSample(
      kClearOnExitSyncEventHistogram,
      PrivacyMetricsService::ClearOnExitSyncEvent::kStartupSyncDisabled, 1);
}
