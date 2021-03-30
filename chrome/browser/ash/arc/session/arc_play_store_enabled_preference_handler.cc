// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_play_store_enabled_preference_handler.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"

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
  VLOG(1) << "Start observing Google Play Store enabled preference. "
          << "Initial value: " << is_play_store_enabled;

  // Force data clean if needed.
  if (IsArcDataCleanupOnStartRequested()) {
    VLOG(1) << "Request to cleanup data on start.";
    arc_session_manager_->RequestArcDataRemoval();
  }

  // If the OOBE is shown, don't kill the mini-container. We'll do it if and
  // when the user declines the TOS. We need to check |is_play_store_enabled| to
  // handle the case where |kArcEnabled| is managed but some of the preferences
  // still need to be set by the user.
  // TODO(cmtm): This feature isn't covered by unittests. Add a unittest for it.
  if (!IsArcOobeOptInActive() || is_play_store_enabled)
    UpdateArcSessionManager();
  if (is_play_store_enabled)
    return;

  // Google Play Store is initially disabled, here.

  if (IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_)) {
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
      // Remove the pinned Play Store icon launcher in Shelf.
      // This is only for non-Managed cases. In managed cases, it is expected
      // to be "disabled" rather than "removed", so keep it here.
      auto* chrome_launcher_controller = ChromeLauncherController::instance();
      if (chrome_launcher_controller)
        chrome_launcher_controller->UnpinAppWithID(kPlayStoreAppId);

      // Tell Consent Auditor that the Play Store consent was revoked.
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile_);
      // TODO(crbug.com/850297): Fix unrelated tests that are not properly
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

  if (ShouldArcAlwaysStart() || IsArcPlayStoreEnabledForProfile(profile_)) {
    arc_session_manager_->RequestEnable();
  } else {
    const bool enable_requested = arc_session_manager_->enable_requested();
    arc_session_manager_->RequestDisable();
    if (enable_requested)
      arc_session_manager_->RequestArcDataRemoval();
  }
}

}  // namespace arc
