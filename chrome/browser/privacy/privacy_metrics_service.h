// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_PRIVACY_METRICS_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_PRIVACY_METRICS_SERVICE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"

class HostContentSettingsMap;
class PrefService;

// Records privacy-related UMA metrics and is created on profile startup. Allows
// consolidation of metrics which do not otherwise have an obvious home, as well
// as recording metrics which span events across multiple disparate locations
// in the browser.
class PrivacyMetricsService : public KeyedService,
                              public syncer::SyncServiceObserver,
                              public signin::IdentityManager::Observer {
 public:
  explicit PrivacyMetricsService(
      PrefService* pref_service,
      HostContentSettingsMap* host_content_settings_map_,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager);
  ~PrivacyMetricsService() override;

  // KeyedService:
  void Shutdown() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

 private:
  friend class PrivacyMetricsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacyMetricsServiceTest, BasicShutdownMetrics);
  FRIEND_TEST_ALL_PREFIXES(PrivacyMetricsServiceTest,
                           FixSyncPausedThroughReLogin);
  FRIEND_TEST_ALL_PREFIXES(PrivacyMetricsServiceTest,
                           FixSyncPausedThroughLogout);
  FRIEND_TEST_ALL_PREFIXES(PrivacyMetricsServiceTest, NoSyncIssues);
  FRIEND_TEST_ALL_PREFIXES(PrivacyMetricsServiceTest, NoSyncIssues);
  FRIEND_TEST_ALL_PREFIXES(PrivacyMetricsServiceTest,
                           AccountChangeNoSyncIssues);
  FRIEND_TEST_ALL_PREFIXES(PrivacyMetricsServiceTest, StartupNoSync);

  // Contains all recorded events related to sync while the profile has clear
  // on exit enabled. Must be kept in sync with the ClearOnExitSyncEvent enum in
  // histograms/enums.xml.
  enum class ClearOnExitSyncEvent {
    kStartupSyncDisabled = 0,
    kStartupSyncPaused = 1,
    kStartupSyncActive = 2,
    kReloginToPausedAccount = 3,
    kLogoutOfPausedAccount = 4,
    kShutdownSyncActiveStartedPausedConsentChange = 5,
    kShutdownSyncActiveStartedPausedNoConsentChange = 6,
    kShutdownSyncActiveStartedActiveConsentChange = 7,
    kShutdownSyncActiveStartedActiveNoConsentChange = 8,
    kShutdownSyncPaused = 9,
    // Add values above this line with a corresponding label in
    // tools/metrics/histograms/enums.xml
    kMaxValue = kShutdownSyncPaused,
  };

  void RecordStartupMetrics();

  void UnregisterObservers();

  // Whether |pref_service_| represents a state where the cookies clear on exit
  // control has been enabled.
  bool IsClearOnExitEnabled();

  void RecordClearOnExitSyncEvent(ClearOnExitSyncEvent event) const;

  // Whether this service observed the |sync_service_| entering the sync
  // paused state before it entered the active state.
  bool sync_started_paused_ = false;

  // Whether this service observed that the sync consent for primary account
  // changed, indicating that a user enabled or disabled sync.
  bool primary_account_consent_changed_ = false;

  const raw_ptr<const PrefService> pref_service_;
  const raw_ptr<const HostContentSettingsMap> host_content_settings_map_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};
};

#endif  // CHROME_BROWSER_PRIVACY_PRIVACY_METRICS_SERVICE_H_
