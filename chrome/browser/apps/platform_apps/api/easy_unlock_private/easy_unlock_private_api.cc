// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/easy_unlock_private/easy_unlock_private_api.h"

#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/api/easy_unlock_private/easy_unlock_private_connection.h"
#include "chrome/browser/apps/platform_apps/api/easy_unlock_private/easy_unlock_private_connection_manager.h"
#include "chrome/browser/chromeos/cryptauth/cryptauth_device_id_provider_impl.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_screenlock_state_handler.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_regular.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_tpm_key_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/proximity_auth/proximity_auth_error_bubble.h"
#include "chrome/common/apps/platform_apps/api/easy_unlock_private.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/proximity_auth/bluetooth_low_energy_setup_connection_finder.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/components/proximity_auth/proximity_auth_client.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/components/proximity_auth/screenlock_state.h"
#include "chromeos/components/proximity_auth/switches.h"
#include "components/account_id/account_id.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/cryptauth_enrollment_utils.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/secure_message_delegate_impl.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

using proximity_auth::ScreenlockState;

namespace chrome_apps {
namespace api {

namespace {

static base::LazyInstance<EasyUnlockPrivateAPI::Factory>::DestructorAtExit
    g_easy_unlock_private_api_factory = LAZY_INSTANCE_INITIALIZER;

EasyUnlockPrivateConnectionManager* GetConnectionManager(
    content::BrowserContext* context) {
  return EasyUnlockPrivateAPI::Factory::Get(context)->get_connection_manager();
}

}  // namespace

// static
EasyUnlockPrivateAPI::Factory* EasyUnlockPrivateAPI::GetFactoryInstance() {
  return g_easy_unlock_private_api_factory.Pointer();
}

EasyUnlockPrivateAPI::EasyUnlockPrivateAPI(content::BrowserContext* context)
    : connection_manager_(new EasyUnlockPrivateConnectionManager(context)) {}

EasyUnlockPrivateAPI::~EasyUnlockPrivateAPI() {}

void EasyUnlockPrivateAPI::Shutdown() {
  // Any dependency which references BrowserContext must be cleaned up here.
  connection_manager_.reset();
}

EasyUnlockPrivateGetStringsFunction::EasyUnlockPrivateGetStringsFunction() {}
EasyUnlockPrivateGetStringsFunction::~EasyUnlockPrivateGetStringsFunction() {}

ExtensionFunction::ResponseAction EasyUnlockPrivateGetStringsFunction::Run() {
  std::unique_ptr<base::DictionaryValue> strings(new base::DictionaryValue);

  const base::string16 device_type = ui::GetChromeOSDeviceName();

  const user_manager::UserManager* manager = user_manager::UserManager::Get();
  const user_manager::User* user = manager ? manager->GetActiveUser() : NULL;
  const std::string user_email_utf8 =
      user ? user->display_email() : std::string();
  const base::string16 user_email = base::UTF8ToUTF16(user_email_utf8);

  // Common strings.
  strings->SetString("learnMoreLinkTitle",
                     l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  strings->SetString("deviceType", device_type);

  // Setup notification strings.
  strings->SetString(
      "setupNotificationTitle",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_TITLE));
  strings->SetString(
      "setupNotificationMessage",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_NOTIFICATION_MESSAGE,
                                 device_type));
  strings->SetString("setupNotificationButtonTitle",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_NOTIFICATION_BUTTON_TITLE));

  // Chromebook added to Easy Unlock notification strings.
  strings->SetString("chromebookAddedNotificationTitle",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_TITLE));
  strings->SetString(
      "chromebookAddedNotificationMessage",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_MESSAGE, device_type));
  strings->SetString(
      "chromebookAddedNotificationAboutButton",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_CHROMEBOOK_ADDED_NOTIFICATION_ABOUT_BUTTON));

  // Shared "Learn more" button for the pairing changed and pairing change
  // applied notification.
  strings->SetString("phoneChangedNotificationLearnMoreButton",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_NOTIFICATION_LEARN_MORE_BUTTON));

  // Pairing changed notification strings.
  strings->SetString("phoneChangedNotificationTitle",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_TITLE));
  strings->SetString(
      "phoneChangedNotificationMessage",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_MESSAGE, device_type));
  strings->SetString(
      "phoneChangedNotificationUpdateButton",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGED_NOTIFICATION_UPDATE_BUTTON));

  // Phone change applied notification strings.
  strings->SetString(
      "phoneChangeAppliedNotificationTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_TITLE));
  strings->SetString(
      "phoneChangeAppliedNotificationMessage",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_PAIRING_CHANGE_APPLIED_NOTIFICATION_MESSAGE));

  // Setup dialog strings.
  // Step 1: Intro.
  strings->SetString(
      "setupIntroHeaderTitle",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_INTRO_HEADER_TITLE,
                                 device_type));
  strings->SetString(
      "setupIntroHeaderText",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SETUP_INTRO_HEADER_TEXT));
  strings->SetString("setupIntroFindPhoneButtonLabel",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_INTRO_FIND_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroFindingPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_FINDING_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroRetryFindPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_RETRY_FIND_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroCloseFindPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_CLOSE_FIND_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupIntroHowIsThisSecureLinkText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_INTRO_HOW_IS_THIS_SECURE_LINK_TEXT));
  // Step 1.5: Phone found but is not secured with lock screen
  strings->SetString("setupSecurePhoneHeaderTitle",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_SECURE_PHONE_HEADER_TITLE));
  strings->SetString(
      "setupSecurePhoneHeaderText",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_SECURE_PHONE_HEADER_TEXT,
                                 device_type));
  strings->SetString("setupSecurePhoneButtonLabel",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_SECURE_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupSecurePhoneLinkText",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SETUP_SECURE_PHONE_LINK_TEXT));
  // Step 2: Found a viable phone.
  strings->SetString(
      "setupFoundPhoneHeaderTitle",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_HEADER_TITLE,
                                 device_type));
  strings->SetString(
      "setupFoundPhoneHeaderText",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_HEADER_TEXT,
                                 device_type));
  strings->SetString(
      "setupFoundPhoneUseThisPhoneButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_USE_THIS_PHONE_BUTTON_LABEL));
  strings->SetString(
      "setupFoundPhoneDeviceFormattedButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_DEVICE_FORMATTED_BUTTON_LABEL));
  strings->SetString(
      "setupFoundPhoneSwitchPhoneLinkLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_FOUND_PHONE_SWITCH_PHONE_LINK_LABEL));
  strings->SetString(
      "setupPairingPhoneFailedButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_PAIRING_PHONE_FAILED_BUTTON_LABEL));
  // Step 2.5: Recommend user to set up Android Smart Lock
  strings->SetString(
      "setupAndroidSmartLockHeaderTitle",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_ANDROID_SMART_LOCK_HEADER_TITLE));
  strings->SetString(
      "setupAndroidSmartLockHeaderText",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_ANDROID_SMART_LOCK_HEADER_TEXT, device_type));
  strings->SetString(
      "setupAndroidSmartLockDoneButtonText",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_ANDROID_SMART_LOCK_DONE_BUTTON_LABEL));
  strings->SetString("setupAndroidSmartLockAboutLinkText",
                     l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  // Step 3: Setup completed successfully.
  strings->SetString(
      "setupCompleteHeaderTitle",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SETUP_COMPLETE_HEADER_TITLE));
  strings->SetString(
      "setupCompleteHeaderText",
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SETUP_COMPLETE_HEADER_TEXT,
                                 device_type));
  strings->SetString(
      "setupCompleteTryItOutButtonLabel",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_COMPLETE_TRY_IT_OUT_BUTTON_LABEL));
  strings->SetString("setupCompleteSettingsLinkText",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_COMPLETE_SETTINGS_LINK_TEXT));
  // Step 4: Post lockscreen confirmation.
  strings->SetString("setupPostLockDismissButtonLabel",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_POST_LOCK_DISMISS_BUTTON_LABEL));

  // Error strings.
  strings->SetString(
      "setupErrorBluetoothUnavailable",
      l10n_util::GetStringFUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_BLUETOOTH_UNAVAILBLE, device_type));
  strings->SetString("setupErrorOffline",
                     l10n_util::GetStringFUTF16(
                         IDS_EASY_UNLOCK_SETUP_ERROR_OFFLINE, device_type));
  strings->SetString(
      "setupErrorRemoteSoftwareOutOfDate",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_REMOTE_SOFTWARE_OUT_OF_DATE));
  strings->SetString(
      "setupErrorRemoteSoftwareOutOfDateGeneric",
      l10n_util::GetStringUTF16(
          IDS_EASY_UNLOCK_SETUP_ERROR_REMOTE_SOFTWARE_OUT_OF_DATE_GENERIC));
  strings->SetString(
      "setupErrorFindingPhone",
      l10n_util::GetStringUTF16(IDS_EASY_UNLOCK_SETUP_ERROR_FINDING_PHONE));
  strings->SetString("setupErrorSyncPhoneState",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_ERROR_SYNC_PHONE_STATE_FAILED));
  strings->SetString("setupErrorConnectingToPhone",
                     l10n_util::GetStringUTF16(
                         IDS_EASY_UNLOCK_SETUP_ERROR_CONNECTING_TO_PHONE));

  return RespondNow(OneArgument(std::move(strings)));
}

EasyUnlockPrivateShowErrorBubbleFunction::
    EasyUnlockPrivateShowErrorBubbleFunction() {}

EasyUnlockPrivateShowErrorBubbleFunction::
    ~EasyUnlockPrivateShowErrorBubbleFunction() {}

ExtensionFunction::ResponseAction
EasyUnlockPrivateShowErrorBubbleFunction::Run() {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents || extensions::GetViewType(web_contents) !=
                           extensions::VIEW_TYPE_APP_WINDOW) {
    return RespondNow(Error("A foreground app window is required."));
  }

  std::unique_ptr<easy_unlock_private::ShowErrorBubble::Params> params(
      easy_unlock_private::ShowErrorBubble::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->link_range.start < 0 || params->link_range.end < 0 ||
      base::saturated_cast<size_t>(params->link_range.end) >
          params->message.size()) {
    return RespondNow(Error("Invalid link range."));
  }

#if defined(TOOLKIT_VIEWS)
  gfx::Rect anchor_rect(params->anchor_rect.left, params->anchor_rect.top,
                        params->anchor_rect.width, params->anchor_rect.height);
  anchor_rect += web_contents->GetContainerBounds().OffsetFromOrigin();
  ShowProximityAuthErrorBubble(
      base::UTF8ToUTF16(params->message),
      gfx::Range(params->link_range.start, params->link_range.end),
      GURL(params->link_target), anchor_rect, web_contents);
  return RespondNow(NoArguments());
#else
  return RespondNow(Error("Not supported on non-Views platforms."));
#endif
}

EasyUnlockPrivateHideErrorBubbleFunction::
    EasyUnlockPrivateHideErrorBubbleFunction() {}

EasyUnlockPrivateHideErrorBubbleFunction::
    ~EasyUnlockPrivateHideErrorBubbleFunction() {}

ExtensionFunction::ResponseAction
EasyUnlockPrivateHideErrorBubbleFunction::Run() {
#if defined(TOOLKIT_VIEWS)
  HideProximityAuthErrorBubble();
  return RespondNow(NoArguments());
#else
  return RespondNow(Error("Not supported on non-Views platforms."));
#endif
}

EasyUnlockPrivateFindSetupConnectionFunction::
    EasyUnlockPrivateFindSetupConnectionFunction() {}

EasyUnlockPrivateFindSetupConnectionFunction::
    ~EasyUnlockPrivateFindSetupConnectionFunction() {
  connection_finder_.reset();
}

void EasyUnlockPrivateFindSetupConnectionFunction::
    OnConnectionFinderTimedOut() {
  connection_finder_.reset();
  Respond(Error("No connection found."));
}

void EasyUnlockPrivateFindSetupConnectionFunction::OnConnectionFound(
    std::unique_ptr<cryptauth::Connection> connection) {
  // Connection are not persistent by default.
  bool persistent = false;
  int connection_id =
      GetConnectionManager(browser_context())
          ->AddConnection(extension(), std::move(connection), persistent);
  Respond(
      ArgumentList(easy_unlock_private::FindSetupConnection::Results::Create(
          connection_id)));
}

ExtensionFunction::ResponseAction
EasyUnlockPrivateFindSetupConnectionFunction::Run() {
  std::unique_ptr<easy_unlock_private::FindSetupConnection::Params> params =
      easy_unlock_private::FindSetupConnection::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Creates a BLE connection finder to look for any device advertising
  // |params->setup_service_uuid|.
  connection_finder_ =
      std::make_unique<proximity_auth::BluetoothLowEnergySetupConnectionFinder>(
          params->setup_service_uuid);

  connection_finder_->Find(base::Bind(
      &EasyUnlockPrivateFindSetupConnectionFunction::OnConnectionFound, this));

  timer_ = std::make_unique<base::OneShotTimer>();
  timer_->Start(FROM_HERE, base::TimeDelta::FromSeconds(params->time_out),
                base::Bind(&EasyUnlockPrivateFindSetupConnectionFunction::
                               OnConnectionFinderTimedOut,
                           this));

  return RespondLater();
}

EasyUnlockPrivateSetupConnectionDisconnectFunction::
    EasyUnlockPrivateSetupConnectionDisconnectFunction() {}

EasyUnlockPrivateSetupConnectionDisconnectFunction::
    ~EasyUnlockPrivateSetupConnectionDisconnectFunction() {}

ExtensionFunction::ResponseAction
EasyUnlockPrivateSetupConnectionDisconnectFunction::Run() {
  std::unique_ptr<easy_unlock_private::SetupConnectionDisconnect::Params>
      params = easy_unlock_private::SetupConnectionDisconnect::Params::Create(
          *args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!GetConnectionManager(browser_context())
           ->Disconnect(extension(), params->connection_id)) {
    return RespondNow(Error("Invalid connectionId."));
  }
  return RespondNow(NoArguments());
}

EasyUnlockPrivateSetupConnectionSendFunction::
    EasyUnlockPrivateSetupConnectionSendFunction() {}

EasyUnlockPrivateSetupConnectionSendFunction::
    ~EasyUnlockPrivateSetupConnectionSendFunction() {}

ExtensionFunction::ResponseAction
EasyUnlockPrivateSetupConnectionSendFunction::Run() {
  std::unique_ptr<easy_unlock_private::SetupConnectionSend::Params> params =
      easy_unlock_private::SetupConnectionSend::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string payload(params->data.begin(), params->data.end());
  if (!GetConnectionManager(browser_context())
           ->SendMessage(extension(), params->connection_id, payload)) {
    return RespondNow(Error("Invalid connectionId."));
  }
  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace chrome_apps

template <>
void chrome_apps::api::EasyUnlockPrivateAPI::Factory::
    DeclareFactoryDependencies() {
  DependsOn(EasyUnlockPrivateConnectionResourceManager::GetFactoryInstance());
}
