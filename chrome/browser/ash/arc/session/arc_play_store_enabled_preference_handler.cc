// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_play_store_enabled_preference_handler.h"

#include <string>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

using sync_pb::UserConsentTypes;

namespace arc {

ArcPlayStoreEnabledPreferenceHandler::ArcPlayStoreEnabledPreferenceHandler(
    Profile* profile,
    ArcSessionManager* arc_session_manager)
    : profile_(profile), arc_session_manager_(arc_session_manager) {
  DCHECK(profile_);
  DCHECK(arc_session_manager_);
}

ArcPlayStoreEnabledPreferenceHandler::~ArcPlayStoreEnabledPreferenceHandler() {
  pref_change_registrar_.RemoveAll();
}

void ArcPlayStoreEnabledPreferenceHandler::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Start observing Google Play Store enabled preference.
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kArcEnabled,
      base::BindRepeating(
          &ArcPlayStoreEnabledPreferenceHandler::OnPreferenceChanged,
          weak_ptr_factory_.GetWeakPtr()));

  const bool is_play_store_enabled = IsArcPlayStoreEnabledForProfile(profile_);
  const bool is_play_store_managed =
      IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_);
  VLOG(1) << "Start observing Google Play Store enabled preference. "
          << "Initial values are: Enabled=" << is_play_store_enabled << " "
          << "Managed=" << is_play_store_managed;

  // Force data clean if needed.
  if (IsArcDataCleanupOnStartRequested()) {
    VLOG(1) << "Request to cleanup data on start.";
    arc_session_manager_->RequestArcDataRemoval();
  }

  // For unmanaged users, if the OOBE is shown we don't kill the
  // mini-container since we want to upgrade it later. If Play Store
  // setting is managed update the state immediately even if the user is
  // in OOBE since we won't get further updates to the setting via OOBE.
  if (!IsArcOobeOptInActive() || is_play_store_managed || is_play_store_enabled)
    UpdateArcSessionManager();

  if (is_play_store_enabled)
    return;

  // Google Play Store is initially disabled, here.

  if (is_play_store_managed) {
    // All users that can disable Google Play Store by themselves will have
    // the |kARcDataRemoveRequested| pref set, so we don't need to eagerly
    // remove the data for that case.
    // For managed users, the preference can change when the Profile object is
    // not alive, so we still need to check it here in case it was disabled to
    // ensure that the data is deleted in case it was disabled between
    // launches.
    VLOG(1) << "Google Play Store is initially disabled for managed "
            << "profile. Removing data.";
    arc_session_manager_->RequestArcDataRemoval();
  }
}

void ArcPlayStoreEnabledPreferenceHandler::OnPreferenceChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const bool is_play_store_enabled = IsArcPlayStoreEnabledForProfile(profile_);
  if (!IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_)) {
    // Update UMA only for non-Managed cases. Note, that multiple OptIn/OptOut
    // may happen during a session. In this case each event would be reported.
    // For example, if a user opts-in ARC on OOBE, and later opts-out via
    // settings page, OOBE_OPTED_IN and SESSION_OPTED_OUT will be recorded.
    if (IsArcOobeOptInActive()) {
      OptInActionType type;
      if (IsArcOobeOptInConfigurationBased()) {
        type = is_play_store_enabled
                   ? OptInActionType::OOBE_OPTED_IN_CONFIGURATION
                   : OptInActionType::OOBE_OPTED_OUT;
      } else {
        type = is_play_store_enabled ? OptInActionType::OOBE_OPTED_IN
                                     : OptInActionType::OOBE_OPTED_OUT;
      }
      UpdateOptInActionUMA(type);
    } else {
      UpdateOptInActionUMA(is_play_store_enabled
                               ? OptInActionType::SESSION_OPTED_IN
                               : OptInActionType::SESSION_OPTED_OUT);
    }

    if (!is_play_store_enabled) {
      // Remove the pinned Play Store icon from the Shelf.
      // This is only for non-Managed cases. In managed cases, it is expected
      // to be "disabled" rather than "removed", so keep it here.
      auto* chrome_shelf_controller = ChromeShelfController::instance();
      if (chrome_shelf_controller)
        chrome_shelf_controller->UnpinAppWithID(kPlayStoreAppId);

      // Tell Consent Auditor that the Play Store consent was revoked.
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile_);
      // TODO(crbug.com/40579665): Fix unrelated tests that are not properly
      // setting up the state of identity_manager and enable the DCHECK instead
      // of the conditional below.
      // DCHECK(identity_manager->HasPrimaryAccount(
      //            signin::ConsentLevel::kSignin));
      if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
        // This class doesn't care about browser sync consent.
        const CoreAccountId account_id = identity_manager->GetPrimaryAccountId(
            signin::ConsentLevel::kSignin);

        UserConsentTypes::ArcPlayTermsOfServiceConsent play_consent;
        play_consent.set_status(UserConsentTypes::NOT_GIVEN);
        play_consent.set_confirmation_grd_id(
            IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_REMOVE);
        play_consent.add_description_grd_ids(
            IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_MESSAGE);
        play_consent.set_consent_flow(
            UserConsentTypes::ArcPlayTermsOfServiceConsent::SETTING_CHANGE);
        ConsentAuditorFactory::GetForProfile(profile_)->RecordArcPlayConsent(
            account_id, play_consent);
      }
    }
  }

  UpdateArcSessionManager();

  // Due to life time management, OnArcPlayStoreEnabledChanged() is notified
  // via ArcSessionManager, so that external services can subscribe at almost
  // any time.
  arc_session_manager_->NotifyArcPlayStoreEnabledChanged(is_play_store_enabled);
}

void ArcPlayStoreEnabledPreferenceHandler::UpdateArcSessionManager() {
  auto* support_host = arc_session_manager_->support_host();
  if (support_host && IsArcPlayStoreEnabledForProfile(profile_)) {
    support_host->SetArcManaged(
        IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_));
  }

  if (ShouldArcAlwaysStart()) {
    arc_session_manager_->AllowActivation(
        ArcSessionManager::AllowActivationReason::kAlwaysStartIsEnabled);
    arc_session_manager_->RequestEnable();
  } else if (IsArcPlayStoreEnabledForProfile(profile_)) {
    if (!ShouldArcStartManually()) {
      arc_session_manager_->RequestEnable();
    } else {
      VLOG(1) << "ARC is not started automatically";
    }
  } else {
    arc_session_manager_->RequestDisableWithArcDataRemoval();
  }
}

}  // namespace arc
