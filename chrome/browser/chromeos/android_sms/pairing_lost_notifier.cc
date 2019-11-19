// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/pairing_lost_notifier.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace chromeos {

namespace android_sms {

namespace {

const char kWasPreviouslySetUpPrefName[] = "android_sms.was_previously_set_up";

const char kAndroidSmsNotifierId[] = "ash.android_sms";
const char kPairingLostNotificationId[] = "android_sms.pairing_lost";

}  // namespace

// static
void PairingLostNotifier::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kWasPreviouslySetUpPrefName, false);
}

PairingLostNotifier::PairingLostNotifier(
    Profile* profile,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    PrefService* pref_service,
    multidevice_setup::AndroidSmsAppHelperDelegate*
        android_sms_app_helper_delegate)
    : profile_(profile),
      multidevice_setup_client_(multidevice_setup_client),
      pref_service_(pref_service),
      android_sms_app_helper_delegate_(android_sms_app_helper_delegate) {
  multidevice_setup_client_->AddObserver(this);
  HandleMessagesFeatureState();
}

PairingLostNotifier::~PairingLostNotifier() {
  multidevice_setup_client_->RemoveObserver(this);
}

void PairingLostNotifier::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  HandleMessagesFeatureState();
}

void PairingLostNotifier::HandleMessagesFeatureState() {
  multidevice_setup::mojom::FeatureState state =
      multidevice_setup_client_->GetFeatureStates()
          .find(multidevice_setup::mojom::Feature::kMessages)
          ->second;

  // If Messages is currently enabled or disabled, the user has completed the
  // setup process.
  if (state == multidevice_setup::mojom::FeatureState::kDisabledByUser ||
      state == multidevice_setup::mojom::FeatureState::kEnabledByUser) {
    HandleSetUpFeatureState();
    return;
  }

  // If further setup is not required, there is no need to show a notification.
  if (state != multidevice_setup::mojom::FeatureState::kFurtherSetupRequired)
    return;

  // The Messages was not previously set up, the notification should not be
  // shown.
  if (!pref_service_->GetBoolean(kWasPreviouslySetUpPrefName))
    return;

  // Set the preference to false to indicate that the app was not previously set
  // up, then show the notification.
  pref_service_->SetBoolean(kWasPreviouslySetUpPrefName, false);
  ShowPairingLostNotification();
}

void PairingLostNotifier::HandleSetUpFeatureState() {
  // Store a preference indicating that the feature has been set up. This
  // preference will be checked in the future in the case that the phone has
  // become unpaired.
  pref_service_->SetBoolean(kWasPreviouslySetUpPrefName, true);

  // If the "pairing lost" notification is currently visible, close it.
  // Otherwise, the user could be confused that a notification is alerting the
  // user to complete a task that has already been completed.
  ClosePairingLostNotificationIfVisible();
}

void PairingLostNotifier::ShowPairingLostNotification() {
  PA_LOG(INFO) << "PairingLostNotifier::ShowPairingLostNotification(): "
               << "Pairing has been lost; displaying notification.";

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT,
      *ash::CreateSystemNotification(
          message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
          kPairingLostNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ANDROID_MESSAGES_PAIRING_LOST_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ANDROID_MESSAGES_PAIRING_LOST_NOTIFICATION_MESSAGE),
          base::string16() /* display_source */, GURL() /* origin_url */,
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kAndroidSmsNotifierId),
          {} /* rich_notification_data */,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &PairingLostNotifier::OnPairingLostNotificationClick,
                  weak_ptr_factory_.GetWeakPtr())),
          kNotificationMessagesIcon,
          message_center::SystemNotificationWarningLevel::NORMAL),
      /*metadata=*/nullptr);
}

void PairingLostNotifier::ClosePairingLostNotificationIfVisible() {
  PA_LOG(INFO) << "PairingLostNotifier::"
               << "ClosePairingLostNotificationIfVisible(): "
               << "Closing pairing lost notification if visible.";

  NotificationDisplayService::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kPairingLostNotificationId);
}

void PairingLostNotifier::OnPairingLostNotificationClick(
    base::Optional<int> button_index) {
  PA_LOG(INFO) << "PairingLostNotifier::OnPairingLostNotificationClick(): "
               << "Pairing notification clicked; opening PWA.";

  ClosePairingLostNotificationIfVisible();
  android_sms_app_helper_delegate_->SetUpAndLaunchAndroidSmsApp();
}

}  // namespace android_sms

}  // namespace chromeos
