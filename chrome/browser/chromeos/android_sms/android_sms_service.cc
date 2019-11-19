// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_service.h"

#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_setup_controller_impl.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/chromeos/android_sms/connection_manager.h"
#include "chrome/browser/chromeos/android_sms/fcm_connection_establisher.h"
#include "chrome/browser/chromeos/android_sms/pairing_lost_notifier.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/storage_partition.h"

namespace chromeos {

namespace android_sms {

AndroidSmsService::AndroidSmsService(
    Profile* profile,
    HostContentSettingsMap* host_content_settings_map,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    web_app::WebAppProvider* web_app_provider,
    app_list::AppListSyncableService* app_list_syncable_service)
    : profile_(profile),
      multidevice_setup_client_(multidevice_setup_client),
      andoid_sms_app_setup_controller_(
          std::make_unique<AndroidSmsAppSetupControllerImpl>(
              profile_,
              &web_app_provider->pending_app_manager(),
              host_content_settings_map)),
      android_sms_app_manager_(std::make_unique<AndroidSmsAppManagerImpl>(
          profile_,
          andoid_sms_app_setup_controller_.get(),
          profile_->GetPrefs(),
          app_list_syncable_service)),
      android_sms_pairing_state_tracker_(
          std::make_unique<AndroidSmsPairingStateTrackerImpl>(
              profile_,
              android_sms_app_manager_.get())),
      pairing_lost_notifier_(std::make_unique<PairingLostNotifier>(
          profile,
          multidevice_setup_client,
          profile_->GetPrefs(),
          android_sms_app_manager_.get())) {
  session_manager::SessionManager::Get()->AddObserver(this);
}

AndroidSmsService::~AndroidSmsService() = default;

void AndroidSmsService::Shutdown() {
  connection_manager_.reset();
  // Note: |pairing_lost_notifier_| holds a reference to
  // |android_sms_app_manager_|, so it should be deleted first.
  pairing_lost_notifier_.reset();
  android_sms_pairing_state_tracker_.reset();
  android_sms_app_manager_.reset();
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void AndroidSmsService::OnSessionStateChanged() {
  // At most one ConnectionManager should be created.
  if (connection_manager_)
    return;

  // ConnectionManager should not be created for blocked sessions.
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked())
    return;

  std::unique_ptr<ConnectionEstablisher> connection_establisher;
  connection_establisher = std::make_unique<FcmConnectionEstablisher>(
      std::make_unique<base::OneShotTimer>());

  connection_manager_ = std::make_unique<ConnectionManager>(
      std::move(connection_establisher), profile_,
      android_sms_app_manager_.get(), multidevice_setup_client_);
}

}  // namespace android_sms

}  // namespace chromeos
