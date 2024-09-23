// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_alert_generator.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace eche_app {

// The screen lock notification
const char kEcheAppScreenLockNotifierId[] =
    "eche_app_notification_ids.screen_lock";
// The notification type from WebUI is CONNECTION_FAILED or CONNECTION_LOST
// allow users to retry.
const char kEcheAppRetryConnectionNotifierId[] =
    "eche_app_notification_ids.retry_connection";
// The notification type from WebUI is DEVICE_IDLE
// allow users to retry.
const char kEcheAppInactivityNotifierId[] =
    "eche_app_notification_ids.inactivity";
// The notification type from WebUI without any actions need to do.
const char kEcheAppFromWebWithoutButtonNotifierId[] =
    "eche_app_notification_ids.from_web_without_button";
// The toast id of EcheApp.
const char kEcheAppToastId[] = "eche_app_toast_id";
// The notification type from WebUI is WIFI_NOT_READY allow users to network
// settings page from settings button.
const char kEcheAppNetworkSettingNotifierId[] =
    "eche_app_notification_ids.network_settings";

// TODO(crbug.com/40785967): This should probably have a ?p=<FEATURE_NAME> at
// some point.
const char kEcheAppLearnMoreUrl[] = "https://support.google.com/chromebook";

EcheAlertGenerator::EcheAlertGenerator(LaunchAppHelper* launch_app_helper,
                                       PrefService* pref_service)
    : launch_app_helper_(launch_app_helper), pref_service_(pref_service) {
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      ash::prefs::kEnableAutoScreenLock,
      base::BindRepeating(&EcheAlertGenerator::OnEnableScreenLockChanged,
                          base::Unretained(this)));
}

EcheAlertGenerator::~EcheAlertGenerator() {
  pref_change_registrar_.RemoveAll();
}

void EcheAlertGenerator::ShowNotification(const std::u16string& title,
                                          const std::u16string& message,
                                          mojom::WebNotificationType type) {
  PA_LOG(INFO) << "echeapi EcheAlertGenerator ShowNotification";
  launch_app_helper_->ShowNotification(
      title, message,
      std::make_unique<LaunchAppHelper::NotificationInfo>(
          LaunchAppHelper::NotificationInfo::Category::kWebUI, type));
}

void EcheAlertGenerator::ShowToast(const std::u16string& text) {
  PA_LOG(INFO) << "echeapi EcheAlertGenerator ShowToast";
  launch_app_helper_->ShowToast(text);
}

void EcheAlertGenerator::Bind(
    mojo::PendingReceiver<mojom::NotificationGenerator> receiver) {
  notification_receiver_.reset();
  notification_receiver_.Bind(std::move(receiver));
}

void EcheAlertGenerator::OnEnableScreenLockChanged() {
  bool lock_screen_enabled =
      pref_service_->GetBoolean(ash::prefs::kEnableAutoScreenLock);
  if (lock_screen_enabled) {
    launch_app_helper_->CloseNotification(kEcheAppScreenLockNotifierId);
  }
}

}  // namespace eche_app
}  // namespace ash
