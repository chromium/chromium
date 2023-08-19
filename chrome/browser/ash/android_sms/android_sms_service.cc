// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/android_sms_service.h"

#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/android_sms/android_sms_app_setup_controller_impl.h"
#include "chrome/browser/ash/android_sms/android_sms_urls.h"
#include "chrome/browser/ash/android_sms/connection_manager.h"
#include "chrome/browser/ash/android_sms/fcm_connection_establisher.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/storage_partition.h"

namespace ash {
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
              &web_app_provider->externally_managed_app_manager(),
              host_content_settings_map)),
      android_sms_app_manager_(std::make_unique<AndroidSmsAppManagerImpl>(
          profile_,
          andoid_sms_app_setup_controller_.get(),
          profile_->GetPrefs(),
          app_list_syncable_service)),
      android_sms_pairing_state_tracker_(
          std::make_unique<AndroidSmsPairingStateTrackerImpl>(
              profile_,
              android_sms_app_manager_.get())) {
  session_manager::SessionManager::Get()->AddObserver(this);
}

AndroidSmsService::~AndroidSmsService() = default;

void AndroidSmsService::Shutdown() {
  connection_manager_.reset();
  android_sms_pairing_state_tracker_.reset();
  android_sms_app_manager_.reset();
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void AndroidSmsService::OnSessionStateChanged() {
  TRACE_EVENT0("login", "AndroidSmsService::OnSessionStateChanged");
  // ConnectionManager should not be created for blocked sessions.
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return;
  }

  // Start Connection if connection manager already exists.
  // This ensures that the service worker connects again and
  // continues to receive messages after unlock.
  if (connection_manager_) {
    connection_manager_->StartConnection();
    return;
  }

  auto connection_establisher = std::make_unique<FcmConnectionEstablisher>(
      std::make_unique<base::OneShotTimer>());

  connection_manager_ = std::make_unique<ConnectionManager>(
      std::move(connection_establisher), profile_,
      android_sms_app_manager_.get(), multidevice_setup_client_);
}

}  // namespace android_sms
}  // namespace ash
