// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy/privacy_metrics_service.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/sync/service/sync_service.h"

PrivacyMetricsService::PrivacyMetricsService(
    PrefService* pref_service,
    HostContentSettingsMap* host_content_settings_map,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager)
    : pref_service_(pref_service),
      host_content_settings_map_(host_content_settings_map),
      sync_service_(sync_service),
      identity_manager_(identity_manager) {
  DCHECK(pref_service_);
  DCHECK(host_content_settings_map_);

  RecordStartupMetrics();

  // Avoid registering any observers if COE is disabled. Events which change
  // COE state will still be caught on shutdown.
  if (!IsClearOnExitEnabled())
    return;

  // Observe the identity manager regardless of sync state to catch changes to
  // sync level consent (e.g. a user enabling sync).
  if (identity_manager_)
    identity_manager_observer_.Observe(identity_manager_.get());

  // Avoid observing the sync service if sync-the-feature is disabled, events
  // this service is interested in will still be caught by the identity manager
  // observer. This service is not interested in sync-the-transport, as for the
  // user visible sync-paused state to occur, sync-the-feature must be enabled.
  if (!sync_service_ || !sync_service_->IsSyncFeatureEnabled()) {
    RecordClearOnExitSyncEvent(ClearOnExitSyncEvent::kStartupSyncDisabled);
    return;
  }

  sync_service_observer_.Observe(sync_service_.get());

  // While this service is brought up with the profile, and thus practically
  // should capture all sync state changes as the sync service starts up,
  // firing the observer here ensures that no state changes are missed.
  OnStateChanged(sync_service_);
}

PrivacyMetricsService::~PrivacyMetricsService() = default;

void PrivacyMetricsService::Shutdown() {
  UnregisterObservers();

  if (!sync_service_ || !IsClearOnExitEnabled()) {
    return;
  }

  if (sync_service_->IsSyncFeatureActive()) {
    ClearOnExitSyncEvent event;
    if (sync_started_paused_ && primary_account_consent_changed_) {
      event =
          ClearOnExitSyncEvent::kShutdownSyncActiveStartedPausedConsentChange;
    } else if (sync_started_paused_ && !primary_account_consent_changed_) {
      event =
          ClearOnExitSyncEvent::kShutdownSyncActiveStartedPausedNoConsentChange;
    } else if (!sync_started_paused_ && primary_account_consent_changed_) {
      event =
          ClearOnExitSyncEvent::kShutdownSyncActiveStartedActiveConsentChange;
    } else {
      DCHECK(!sync_started_paused_ && !primary_account_consent_changed_);
      event =
          ClearOnExitSyncEvent::kShutdownSyncActiveStartedActiveNoConsentChange;
    }
    RecordClearOnExitSyncEvent(event);
  } else if (sync_service_->GetTransportState() ==
             syncer::SyncService::TransportState::PAUSED) {
    RecordClearOnExitSyncEvent(ClearOnExitSyncEvent::kShutdownSyncPaused);
  }
}

void PrivacyMetricsService::OnStateChanged(syncer::SyncService* sync) {
  // This function will be called in response to all sync service state changes
  // until sync reaches the active state and the observer is unregistered below.
  // This may be soon after startup, or delayed if sync first enters the sync
  // paused state (or experiences another uninteresting sync issue). This
  // includes the initial state sync is in when this service is created, which
  // as this service is created along with the profile is early enough that the
  // user cannot have rectified a sync paused state.

  if (sync->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED &&
      IsClearOnExitEnabled() && !sync_started_paused_) {
    // Sync has started up directly into a paused state, very likely because of
    // the users COE setting. Sync has not previously reached the active state,
    // because when it does this observer is unregistered.
    RecordClearOnExitSyncEvent(ClearOnExitSyncEvent::kStartupSyncPaused);

    // Continue to observe the sync service, as if the user rectifies the
    // sync paused error by signing directly back in, the account change
    // observer wont trigger, but sync will eventually reach an active state.
    sync_started_paused_ = true;
  } else if (sync->GetTransportState() ==
             syncer::SyncService::TransportState::ACTIVE) {
    // Getting into the sync active state by changing primary account is handled
    // by the identity manager observer.
    if (!primary_account_consent_changed_ && IsClearOnExitEnabled()) {
      if (sync_started_paused_) {
        RecordClearOnExitSyncEvent(

            ClearOnExitSyncEvent::kReloginToPausedAccount);
      } else {
        // User managed to avoid the sync paused state while having CoE enabled.
        RecordClearOnExitSyncEvent(ClearOnExitSyncEvent::kStartupSyncActive);
      }
    }
    // No further sync or identity updates are interesting. The final state of
    // sync and COE will however be checked on Shutdown().
    UnregisterObservers();
  }
}

void PrivacyMetricsService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  auto previous_consent_level = event_details.GetPreviousState().consent_level;
  auto current_consent_level = event_details.GetCurrentState().consent_level;

  if (current_consent_level != previous_consent_level) {
    primary_account_consent_changed_ = true;
    if (current_consent_level == signin::ConsentLevel::kSignin &&
        IsClearOnExitEnabled() && sync_started_paused_) {
      DCHECK(previous_consent_level == signin::ConsentLevel::kSync);
      // The only way for a user to downgrade the consent level in the UI is
      // by logging out of the account.
      RecordClearOnExitSyncEvent(ClearOnExitSyncEvent::kLogoutOfPausedAccount);
      identity_manager_observer_.Reset();
    }
  }
}

void PrivacyMetricsService::RecordStartupMetrics() {
  base::UmaHistogramBoolean(
      "Privacy.DoNotTrackSetting2",
      pref_service_->GetBoolean(prefs::kEnableDoNotTrack));

  base::UmaHistogramEnumeration("Settings.PreloadStatus.OnStartup3",
                                prefetch::GetPreloadPagesState(*pref_service_));
  base::UmaHistogramBoolean(
      "Settings.AutocompleteSearches.OnStartup2",
      pref_service_->GetBoolean(::prefs::kSearchSuggestEnabled));

  base::UmaHistogramBoolean(
      "Settings.AdvancedSpellcheck.OnStartup2",
      pref_service_->GetBoolean(
          ::spellcheck::prefs::kSpellCheckUseSpellingService));
}

void PrivacyMetricsService::UnregisterObservers() {
  sync_service_observer_.Reset();
  identity_manager_observer_.Reset();
}

bool PrivacyMetricsService::IsClearOnExitEnabled() {
  return host_content_settings_map_->GetDefaultContentSetting(
             ContentSettingsType::COOKIES) ==
         ContentSetting::CONTENT_SETTING_SESSION_ONLY;
}

void PrivacyMetricsService::RecordClearOnExitSyncEvent(
    ClearOnExitSyncEvent event) const {
  base::UmaHistogramEnumeration("Privacy.ClearOnExitSyncEvent", event);
}
