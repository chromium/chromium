// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_service_impl.h"

#include <map>
#include <sstream>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base64url.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_background_services_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/PushMessagingServiceObserver_jni.h"
#endif

using instance_id::InstanceID;

namespace {

// Scope passed to getToken to obtain GCM registration tokens.
// Must match Java GoogleCloudMessaging.INSTANCE_ID_SCOPE.
const char kGCMScope[] = "GCM";

const int kMaxRegistrations = 1000000;

// Chrome does not yet support silent push messages, and requires websites to
// indicate that they will only send user-visible messages.
const char kSilentPushUnsupportedMessage[] =
    "Chrome currently only supports the Push API for subscriptions that will "
    "result in user-visible messages. You can indicate this by calling "
    "pushManager.subscribe({userVisibleOnly: true}) instead. See "
    "https://goo.gl/yqv4Q4 for more details.";

void RecordDeliveryStatus(blink::mojom::PushDeliveryStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.DeliveryStatus", status);
}

void RecordUnsubscribeReason(blink::mojom::PushUnregistrationReason reason) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UnregistrationReason", reason);
}

void RecordUnsubscribeGCMResult(gcm::GCMClient::Result result) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UnregistrationGCMResult", result,
                            gcm::GCMClient::LAST_RESULT + 1);
}

void RecordUnsubscribeIIDResult(InstanceID::Result result) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UnregistrationIIDResult", result,
                            InstanceID::LAST_RESULT + 1);
}

blink::mojom::PermissionStatus ToPermissionStatus(
    ContentSetting content_setting) {
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      return blink::mojom::PermissionStatus::GRANTED;
    case CONTENT_SETTING_BLOCK:
      return blink::mojom::PermissionStatus::DENIED;
    case CONTENT_SETTING_ASK:
      return blink::mojom::PermissionStatus::ASK;
    default:
      break;
  }
  NOTREACHED();
  return blink::mojom::PermissionStatus::DENIED;
}

void UnregisterCallbackToClosure(
    base::OnceClosure closure,
    blink::mojom::PushUnregistrationStatus status) {
  DCHECK(closure);
  std::move(closure).Run();
}

void LogMessageReceivedEventToDevTools(
    content::DevToolsBackgroundServicesContext* devtools_context,
    const PushMessagingAppIdentifier& app_identifier,
    const std::string& message_id,
    bool was_encrypted,
    const std::string& error_message,
    const std::string& payload) {
  if (!devtools_context)
    return;

  std::map<std::string, std::string> event_metadata = {
      {"Success", error_message.empty() ? "Yes" : "No"},
      {"Was Encrypted", was_encrypted ? "Yes" : "No"}};

  if (!error_message.empty())
    event_metadata["Error Reason"] = error_message;
  else if (was_encrypted)
    event_metadata["Payload"] = payload;

  devtools_context->LogBackgroundServiceEvent(
      app_identifier.service_worker_registration_id(),
      url::Origin::Create(app_identifier.origin()),
      content::DevToolsBackgroundService::kPushMessaging,
      "Push message received" /* event_name */, message_id, event_metadata);
}

}  // namespace

// static
void PushMessagingServiceImpl::InitializeForProfile(Profile* profile) {
  // TODO(johnme): Consider whether push should be enabled in incognito.
  if (!profile || profile->IsOffTheRecord())
    return;

  int count = PushMessagingAppIdentifier::GetCount(profile);
  if (count <= 0)
    return;

  PushMessagingServiceImpl* push_service =
      PushMessagingServiceFactory::GetForProfile(profile);
  if (push_service)
    push_service->IncreasePushSubscriptionCount(count, false /* is_pending */);
}

PushMessagingServiceImpl::PushMessagingServiceImpl(Profile* profile)
    : profile_(profile),
      push_subscription_count_(0),
      pending_push_subscription_count_(0),
      notification_manager_(profile) {
  DCHECK(profile);
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

PushMessagingServiceImpl::~PushMessagingServiceImpl() = default;

void PushMessagingServiceImpl::IncreasePushSubscriptionCount(int add,
                                                             bool is_pending) {
  DCHECK_GT(add, 0);
  if (push_subscription_count_ + pending_push_subscription_count_ == 0)
    GetGCMDriver()->AddAppHandler(kPushMessagingAppIdentifierPrefix, this);

  if (is_pending)
    pending_push_subscription_count_ += add;
  else
    push_subscription_count_ += add;
}

void PushMessagingServiceImpl::DecreasePushSubscriptionCount(int subtract,
                                                             bool was_pending) {
  DCHECK_GT(subtract, 0);
  if (was_pending) {
    pending_push_subscription_count_ -= subtract;
    DCHECK_GE(pending_push_subscription_count_, 0);
  } else {
    push_subscription_count_ -= subtract;
    DCHECK_GE(push_subscription_count_, 0);
  }

  if (push_subscription_count_ + pending_push_subscription_count_ == 0)
    GetGCMDriver()->RemoveAppHandler(kPushMessagingAppIdentifierPrefix);
}

bool PushMessagingServiceImpl::CanHandle(const std::string& app_id) const {
  return base::StartsWith(app_id, kPushMessagingAppIdentifierPrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

void PushMessagingServiceImpl::ShutdownHandler() {
  // Shutdown() should come before and it removes us from the list of app
  // handlers of gcm::GCMDriver so this shouldn't ever been called.
  NOTREACHED();
}

void PushMessagingServiceImpl::OnStoreReset() {
  // Delete all cached subscriptions, since they are now invalid.
  for (const auto& identifier : PushMessagingAppIdentifier::GetAll(profile_)) {
    RecordUnsubscribeReason(
        blink::mojom::PushUnregistrationReason::GCM_STORE_RESET);
    // Clear all the subscriptions in parallel, to reduce risk that shutdown
    // occurs before we finish clearing them.
    ClearPushSubscriptionId(profile_, identifier.origin(),
                            identifier.service_worker_registration_id(),
                            base::DoNothing());
    // TODO(johnme): Fire pushsubscriptionchange/pushsubscriptionlost SW event.
  }
  PushMessagingAppIdentifier::DeleteAllFromPrefs(profile_);
}

// OnMessage methods -----------------------------------------------------------

void PushMessagingServiceImpl::OnMessage(const std::string& app_id,
                                         const gcm::IncomingMessage& message) {
  // We won't have time to process and act on the message.
  // TODO(peter) This should be checked at the level of the GCMDriver, so that
  // the message is not consumed. See https://crbug.com/612815
  if (g_browser_process->IsShuttingDown() || shutdown_started_)
    return;

  in_flight_message_deliveries_.insert(app_id);

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  if (g_browser_process->background_mode_manager()) {
    UMA_HISTOGRAM_BOOLEAN("PushMessaging.ReceivedMessageInBackground",
                          g_browser_process->background_mode_manager()
                              ->IsBackgroundWithoutWindows());
  }

  if (!in_flight_keep_alive_) {
    in_flight_keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::IN_FLIGHT_PUSH_MESSAGE,
        KeepAliveRestartOption::DISABLED);
  }
#endif

  base::Closure message_handled_closure =
      message_callback_for_testing_.is_null() ? base::DoNothing()
                                              : message_callback_for_testing_;
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(profile_, app_id);
  // Drop message and unregister if app_id was unknown (maybe recently deleted).
  if (app_identifier.is_null()) {
    DeliverMessageCallback(app_id, GURL::EmptyGURL(),
                           -1 /* kInvalidServiceWorkerRegistrationId */,
                           message, message_handled_closure,
                           blink::mojom::PushDeliveryStatus::UNKNOWN_APP_ID);
    return;
  }

  LogMessageReceivedEventToDevTools(
      GetDevToolsContext(app_identifier.origin()), app_identifier,
      message.message_id,
      /* was_encrypted= */ message.decrypted, std::string() /* error_message */,
      message.decrypted ? message.raw_data : std::string());

  // Drop message and unregister if |origin| has lost push permission.
  if (!IsPermissionSet(app_identifier.origin())) {
    DeliverMessageCallback(app_id, app_identifier.origin(),
                           app_identifier.service_worker_registration_id(),
                           message, message_handled_closure,
                           blink::mojom::PushDeliveryStatus::PERMISSION_DENIED);
    return;
  }

  rappor::SampleDomainAndRegistryFromGURL(
      g_browser_process->rappor_service(),
      "PushMessaging.MessageReceived.Origin", app_identifier.origin());

  // The payload of a push message can be valid with content, valid with empty
  // content, or null.
  base::Optional<std::string> payload;
  if (message.decrypted)
    payload = message.raw_data;

  // Dispatch the message to the appropriate Service Worker.
  content::BrowserContext::DeliverPushMessage(
      profile_, app_identifier.origin(),
      app_identifier.service_worker_registration_id(), message.message_id,
      payload,
      base::Bind(&PushMessagingServiceImpl::DeliverMessageCallback,
                 weak_factory_.GetWeakPtr(), app_identifier.app_id(),
                 app_identifier.origin(),
                 app_identifier.service_worker_registration_id(), message,
                 message_handled_closure));

  // Inform tests observing message dispatching about the event.
  if (!message_dispatched_callback_for_testing_.is_null()) {
    message_dispatched_callback_for_testing_.Run(
        app_id, app_identifier.origin(),
        app_identifier.service_worker_registration_id(), std::move(payload));
  }
}

void PushMessagingServiceImpl::DeliverMessageCallback(
    const std::string& app_id,
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const gcm::IncomingMessage& message,
    const base::Closure& message_handled_closure,
    blink::mojom::PushDeliveryStatus status) {
  DCHECK_GE(in_flight_message_deliveries_.count(app_id), 1u);

  RecordDeliveryStatus(status);

  base::RepeatingClosure completion_closure = base::BindRepeating(
      &PushMessagingServiceImpl::DidHandleMessage, weak_factory_.GetWeakPtr(),
      app_id, message.message_id, message_handled_closure,
      false /* did_show_generic_notification */);
  // The |completion_closure| should run by default at the end of this function,
  // unless it is explicitly passed to another function or disabled.
  base::ScopedClosureRunner completion_closure_runner(completion_closure);

  // A reason to automatically unsubscribe. UNKNOWN means do not unsubscribe.
  blink::mojom::PushUnregistrationReason unsubscribe_reason =
      blink::mojom::PushUnregistrationReason::UNKNOWN;

  // TODO(mvanouwerkerk): Show a warning in the developer console of the
  // Service Worker corresponding to app_id (and/or on an internals page).
  // See https://crbug.com/508516 for options.
  switch (status) {
    // Call EnforceUserVisibleOnlyRequirements if the message was delivered to
    // the Service Worker JavaScript, even if the website's event handler failed
    // (to prevent sites deliberately failing in order to avoid having to show
    // notifications).
    case blink::mojom::PushDeliveryStatus::SUCCESS:
    case blink::mojom::PushDeliveryStatus::EVENT_WAITUNTIL_REJECTED:
    case blink::mojom::PushDeliveryStatus::TIMEOUT:
      // Only enforce the user visible requirements if this is currently running
      // as the delivery callback for the last in-flight message, and silent
      // push has not been enabled through a command line flag.
      if (in_flight_message_deliveries_.count(app_id) == 1 &&
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAllowSilentPush)) {
        notification_manager_.EnforceUserVisibleOnlyRequirements(
            requesting_origin, service_worker_registration_id,
            base::BindOnce(&PushMessagingServiceImpl::DidHandleMessage,
                           weak_factory_.GetWeakPtr(), app_id,
                           message.message_id, message_handled_closure));
        // Disable the default completion closure.
        completion_closure_runner.ReplaceClosure(base::DoNothing());
      }
      break;
    case blink::mojom::PushDeliveryStatus::SERVICE_WORKER_ERROR:
      // Do nothing, and hope the error is transient.
      break;
    case blink::mojom::PushDeliveryStatus::UNKNOWN_APP_ID:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::DELIVERY_UNKNOWN_APP_ID;
      break;
    case blink::mojom::PushDeliveryStatus::PERMISSION_DENIED:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::DELIVERY_PERMISSION_DENIED;
      break;
    case blink::mojom::PushDeliveryStatus::NO_SERVICE_WORKER:
      unsubscribe_reason =
          blink::mojom::PushUnregistrationReason::DELIVERY_NO_SERVICE_WORKER;
      break;
  }

  if (unsubscribe_reason != blink::mojom::PushUnregistrationReason::UNKNOWN) {
    PushMessagingAppIdentifier app_identifier =
        PushMessagingAppIdentifier::FindByAppId(profile_, app_id);
    UnsubscribeInternal(
        unsubscribe_reason,
        app_identifier.is_null() ? GURL::EmptyGURL() : app_identifier.origin(),
        app_identifier.is_null()
            ? -1 /* kInvalidServiceWorkerRegistrationId */
            : app_identifier.service_worker_registration_id(),
        app_id, message.sender_id,
        base::BindOnce(&UnregisterCallbackToClosure,
                       completion_closure_runner.Release()));

    if (app_identifier.is_null())
      return;

    if (auto* devtools_context = GetDevToolsContext(app_identifier.origin())) {
      std::stringstream ss;
      ss << unsubscribe_reason;
      devtools_context->LogBackgroundServiceEvent(
          app_identifier.service_worker_registration_id(),
          url::Origin::Create(app_identifier.origin()),
          content::DevToolsBackgroundService::kPushMessaging,
          "Unsubscribed due to error" /* event_name */, message.message_id,
          {{"Reason", ss.str()}});
    }
  }
}

void PushMessagingServiceImpl::DidHandleMessage(
    const std::string& app_id,
    const std::string& push_message_id,
    const base::RepeatingClosure& message_handled_closure,
    bool did_show_generic_notification) {
  auto in_flight_iterator = in_flight_message_deliveries_.find(app_id);
  DCHECK(in_flight_iterator != in_flight_message_deliveries_.end());

  // Remove a single in-flight delivery for |app_id|. This has to be done using
  // an iterator rather than by value, as the latter removes all entries.
  in_flight_message_deliveries_.erase(in_flight_iterator);

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  // Reset before running callbacks below, so tests can verify keep-alive reset.
  if (in_flight_message_deliveries_.empty())
    in_flight_keep_alive_.reset();
#endif

  message_handled_closure.Run();

#if defined(OS_ANDROID)
  chrome::android::Java_PushMessagingServiceObserver_onMessageHandled(
      base::android::AttachCurrentThread());
#endif

  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(profile_, app_id);

  if (app_identifier.is_null() || !did_show_generic_notification)
    return;

  if (auto* devtools_context = GetDevToolsContext(app_identifier.origin())) {
    devtools_context->LogBackgroundServiceEvent(
        app_identifier.service_worker_registration_id(),
        url::Origin::Create(app_identifier.origin()),
        content::DevToolsBackgroundService::kPushMessaging,
        "Generic notification shown" /* event_name */, push_message_id,
        {} /* event_metadata */);
  }
}

void PushMessagingServiceImpl::SetMessageCallbackForTesting(
    const base::Closure& callback) {
  message_callback_for_testing_ = callback;
}

// Other gcm::GCMAppHandler methods --------------------------------------------

void PushMessagingServiceImpl::OnMessagesDeleted(const std::string& app_id) {
  // TODO(mvanouwerkerk): Consider firing an event on the Service Worker
  // corresponding to |app_id| to inform the app about deleted messages.
}

void PushMessagingServiceImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

void PushMessagingServiceImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

void PushMessagingServiceImpl::OnMessageDecryptionFailed(
    const std::string& app_id,
    const std::string& message_id,
    const std::string& error_message) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(profile_, app_id);

  if (app_identifier.is_null())
    return;

  LogMessageReceivedEventToDevTools(
      GetDevToolsContext(app_identifier.origin()), app_identifier, message_id,
      /* was_encrypted= */ true, error_message, "" /* payload */);
}

// Subscribe and GetPermissionStatus methods -----------------------------------

void PushMessagingServiceImpl::SubscribeFromDocument(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    int renderer_id,
    int render_frame_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    RegisterCallback callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          profile_, requesting_origin, service_worker_registration_id);

  // If there is no existing app identifier for the given Service Worker,
  // generate a new one. This will create a new subscription on the server.
  if (app_identifier.is_null()) {
    app_identifier = PushMessagingAppIdentifier::Generate(
        requesting_origin, service_worker_registration_id);
  }

  if (push_subscription_count_ + pending_push_subscription_count_ >=
      kMaxRegistrations) {
    SubscribeEndWithError(std::move(callback),
                          blink::mojom::PushRegistrationStatus::LIMIT_REACHED);
    return;
  }

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(renderer_id, render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  if (!options->user_visible_only) {
    web_contents->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        kSilentPushUnsupportedMessage);

    SubscribeEndWithError(
        std::move(callback),
        blink::mojom::PushRegistrationStatus::PERMISSION_DENIED);
    return;
  }

  // Push does not allow permission requests from iframes.
  PermissionManager::Get(profile_)->RequestPermission(
      ContentSettingsType::NOTIFICATIONS, render_frame_host, requesting_origin,
      user_gesture,
      base::BindOnce(&PushMessagingServiceImpl::DoSubscribe,
                     weak_factory_.GetWeakPtr(), app_identifier,
                     std::move(options), std::move(callback)));
}

void PushMessagingServiceImpl::SubscribeFromWorker(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    RegisterCallback register_callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          profile_, requesting_origin, service_worker_registration_id);

  // If there is no existing app identifier for the given Service Worker,
  // generate a new one. This will create a new subscription on the server.
  if (app_identifier.is_null()) {
    app_identifier = PushMessagingAppIdentifier::Generate(
        requesting_origin, service_worker_registration_id);
  }

  if (push_subscription_count_ + pending_push_subscription_count_ >=
      kMaxRegistrations) {
    SubscribeEndWithError(std::move(register_callback),
                          blink::mojom::PushRegistrationStatus::LIMIT_REACHED);
    return;
  }

  blink::mojom::PermissionStatus permission_status =
      GetPermissionStatus(requesting_origin, options->user_visible_only);

  if (permission_status != blink::mojom::PermissionStatus::GRANTED) {
    SubscribeEndWithError(
        std::move(register_callback),
        blink::mojom::PushRegistrationStatus::PERMISSION_DENIED);
    return;
  }

  DoSubscribe(app_identifier, std::move(options), std::move(register_callback),
              CONTENT_SETTING_ALLOW);
}

blink::mojom::PermissionStatus PushMessagingServiceImpl::GetPermissionStatus(
    const GURL& origin,
    bool user_visible) {
  if (!user_visible)
    return blink::mojom::PermissionStatus::DENIED;

  // Because the Push API is tied to Service Workers, many usages of the API
  // won't have an embedding origin at all. Only consider the requesting
  // |origin| when checking whether permission to use the API has been granted.
  return ToPermissionStatus(
      PermissionManager::Get(profile_)
          ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS, origin,
                                origin)
          .content_setting);
}

bool PushMessagingServiceImpl::SupportNonVisibleMessages() {
  return false;
}

void PushMessagingServiceImpl::DoSubscribe(
    const PushMessagingAppIdentifier& app_identifier,
    blink::mojom::PushSubscriptionOptionsPtr options,
    RegisterCallback register_callback,
    ContentSetting content_setting) {
  if (content_setting != CONTENT_SETTING_ALLOW) {
    SubscribeEndWithError(
        std::move(register_callback),
        blink::mojom::PushRegistrationStatus::PERMISSION_DENIED);
    return;
  }

  IncreasePushSubscriptionCount(1, true /* is_pending */);

  std::string application_server_key_string(
      options->application_server_key.begin(),
      options->application_server_key.end());
  GetInstanceIDDriver()
      ->GetInstanceID(app_identifier.app_id())
      ->GetToken(NormalizeSenderInfo(application_server_key_string), kGCMScope,
                 std::map<std::string, std::string>() /* options */,
                 {} /* flags */,
                 base::BindOnce(&PushMessagingServiceImpl::DidSubscribe,
                                weak_factory_.GetWeakPtr(), app_identifier,
                                application_server_key_string,
                                std::move(register_callback)));
}

void PushMessagingServiceImpl::SubscribeEnd(
    RegisterCallback callback,
    const std::string& subscription_id,
    const GURL& endpoint,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth,
    blink::mojom::PushRegistrationStatus status) {
  std::move(callback).Run(subscription_id, endpoint, p256dh, auth, status);
}

void PushMessagingServiceImpl::SubscribeEndWithError(
    RegisterCallback callback,
    blink::mojom::PushRegistrationStatus status) {
  SubscribeEnd(std::move(callback), std::string() /* subscription_id */,
               GURL::EmptyGURL() /* endpoint */,
               std::vector<uint8_t>() /* p256dh */,
               std::vector<uint8_t>() /* auth */, status);
}

void PushMessagingServiceImpl::DidSubscribe(
    const PushMessagingAppIdentifier& app_identifier,
    const std::string& sender_id,
    RegisterCallback callback,
    const std::string& subscription_id,
    InstanceID::Result result) {
  DecreasePushSubscriptionCount(1, true /* was_pending */);

  blink::mojom::PushRegistrationStatus status =
      blink::mojom::PushRegistrationStatus::SERVICE_ERROR;

  switch (result) {
    case InstanceID::SUCCESS: {
      const GURL endpoint = CreateEndpoint(subscription_id);

      // Make sure that this subscription has associated encryption keys prior
      // to returning it to the developer - they'll need this information in
      // order to send payloads to the user.
      GetEncryptionInfoForAppId(
          app_identifier.app_id(), sender_id,
          base::BindOnce(
              &PushMessagingServiceImpl::DidSubscribeWithEncryptionInfo,
              weak_factory_.GetWeakPtr(), app_identifier, std::move(callback),
              subscription_id, endpoint));
      return;
    }
    case InstanceID::INVALID_PARAMETER:
    case InstanceID::DISABLED:
    case InstanceID::ASYNC_OPERATION_PENDING:
    case InstanceID::SERVER_ERROR:
    case InstanceID::UNKNOWN_ERROR:
      DLOG(ERROR) << "Push messaging subscription failed; InstanceID::Result = "
                  << result;
      status = blink::mojom::PushRegistrationStatus::SERVICE_ERROR;
      break;
    case InstanceID::NETWORK_ERROR:
      status = blink::mojom::PushRegistrationStatus::NETWORK_ERROR;
      break;
  }

  SubscribeEndWithError(std::move(callback), status);
}

void PushMessagingServiceImpl::DidSubscribeWithEncryptionInfo(
    const PushMessagingAppIdentifier& app_identifier,
    RegisterCallback callback,
    const std::string& subscription_id,
    const GURL& endpoint,
    std::string p256dh,
    std::string auth_secret) {
  if (p256dh.empty()) {
    SubscribeEndWithError(
        std::move(callback),
        blink::mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE);
    return;
  }

  app_identifier.PersistToPrefs(profile_);

  IncreasePushSubscriptionCount(1, false /* is_pending */);

  SubscribeEnd(std::move(callback), subscription_id, endpoint,
               std::vector<uint8_t>(p256dh.begin(), p256dh.end()),
               std::vector<uint8_t>(auth_secret.begin(), auth_secret.end()),
               blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE);
}

// GetSubscriptionInfo methods -------------------------------------------------

void PushMessagingServiceImpl::GetSubscriptionInfo(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    const std::string& subscription_id,
    const SubscriptionInfoCallback& callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          profile_, origin, service_worker_registration_id);

  if (app_identifier.is_null()) {
    callback.Run(false /* is_valid */, GURL::EmptyGURL() /*endpoint*/,
                 std::vector<uint8_t>() /* p256dh */,
                 std::vector<uint8_t>() /* auth */);
    return;
  }

  const GURL endpoint = CreateEndpoint(subscription_id);
  const std::string& app_id = app_identifier.app_id();
  base::Callback<void(bool)> validate_cb = base::Bind(
      &PushMessagingServiceImpl::DidValidateSubscription,
      weak_factory_.GetWeakPtr(), app_id, sender_id, endpoint, callback);

  if (PushMessagingAppIdentifier::UseInstanceID(app_id)) {
    GetInstanceIDDriver()->GetInstanceID(app_id)->ValidateToken(
        NormalizeSenderInfo(sender_id), kGCMScope, subscription_id,
        validate_cb);
  } else {
    GetGCMDriver()->ValidateRegistration(
        app_id, {NormalizeSenderInfo(sender_id)}, subscription_id, validate_cb);
  }
}

void PushMessagingServiceImpl::DidValidateSubscription(
    const std::string& app_id,
    const std::string& sender_id,
    const GURL& endpoint,
    const SubscriptionInfoCallback& callback,
    bool is_valid) {
  if (!is_valid) {
    callback.Run(false /* is_valid */, GURL::EmptyGURL() /* endpoint */,
                 std::vector<uint8_t>() /* p256dh */,
                 std::vector<uint8_t>() /* auth */);
    return;
  }

  GetEncryptionInfoForAppId(
      app_id, sender_id,
      base::BindOnce(&PushMessagingServiceImpl::DidGetEncryptionInfo,
                     weak_factory_.GetWeakPtr(), endpoint, callback));
}

void PushMessagingServiceImpl::DidGetEncryptionInfo(
    const GURL& endpoint,
    const SubscriptionInfoCallback& callback,
    std::string p256dh,
    std::string auth_secret) const {
  // I/O errors might prevent the GCM Driver from retrieving a key-pair.
  bool is_valid = !p256dh.empty();
  callback.Run(is_valid, endpoint,
               std::vector<uint8_t>(p256dh.begin(), p256dh.end()),
               std::vector<uint8_t>(auth_secret.begin(), auth_secret.end()));
}

// Unsubscribe methods ---------------------------------------------------------

void PushMessagingServiceImpl::Unsubscribe(
    blink::mojom::PushUnregistrationReason reason,
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    UnregisterCallback callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          profile_, requesting_origin, service_worker_registration_id);

  UnsubscribeInternal(
      reason, requesting_origin, service_worker_registration_id,
      app_identifier.is_null() ? std::string() : app_identifier.app_id(),
      sender_id, std::move(callback));
}

void PushMessagingServiceImpl::UnsubscribeInternal(
    blink::mojom::PushUnregistrationReason reason,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& app_id,
    const std::string& sender_id,
    UnregisterCallback callback) {
  DCHECK(!app_id.empty() || (!origin.is_empty() &&
                             service_worker_registration_id !=
                                 -1 /* kInvalidServiceWorkerRegistrationId */))
      << "Need an app_id and/or origin+service_worker_registration_id";

  RecordUnsubscribeReason(reason);

  if (origin.is_empty() ||
      service_worker_registration_id ==
          -1 /* kInvalidServiceWorkerRegistrationId */) {
    // Can't clear Service Worker database.
    DidClearPushSubscriptionId(reason, app_id, sender_id, std::move(callback));
    return;
  }
  ClearPushSubscriptionId(
      profile_, origin, service_worker_registration_id,
      base::BindOnce(&PushMessagingServiceImpl::DidClearPushSubscriptionId,
                     weak_factory_.GetWeakPtr(), reason, app_id, sender_id,
                     std::move(callback)));
}

void PushMessagingServiceImpl::DidClearPushSubscriptionId(
    blink::mojom::PushUnregistrationReason reason,
    const std::string& app_id,
    const std::string& sender_id,
    UnregisterCallback callback) {
  if (app_id.empty()) {
    // Without an |app_id|, we can neither delete the subscription from the
    // PushMessagingAppIdentifier map, nor unsubscribe with the GCM Driver.
    std::move(callback).Run(
        blink::mojom::PushUnregistrationStatus::SUCCESS_WAS_NOT_REGISTERED);
    return;
  }

  // Delete the mapping for this app_id, to guarantee that no messages get
  // delivered in future (even if unregistration fails).
  // TODO(johnme): Instead of deleting these app ids, store them elsewhere, and
  // retry unregistration if it fails due to network errors (crbug.com/465399).
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(profile_, app_id);
  bool was_subscribed = !app_identifier.is_null();
  if (was_subscribed)
    app_identifier.DeleteFromPrefs(profile_);

  // Run the unsubscribe callback *before* asking the InstanceIDDriver/GCMDriver
  // to unsubscribe, since that's a slow process involving network retries, and
  // by this point enough local state has been deleted that the subscription is
  // inactive. Note that DeliverMessageCallback automatically unsubscribes if
  // messages are later received for a subscription that was locally deleted,
  // so as long as messages keep getting sent to it, the unsubscription should
  // eventually reach GCM servers even if this particular attempt fails.
  std::move(callback).Run(
      was_subscribed
          ? blink::mojom::PushUnregistrationStatus::SUCCESS_UNREGISTERED
          : blink::mojom::PushUnregistrationStatus::SUCCESS_WAS_NOT_REGISTERED);

  if (PushMessagingAppIdentifier::UseInstanceID(app_id)) {
    GetInstanceIDDriver()->GetInstanceID(app_id)->DeleteID(
        base::Bind(&PushMessagingServiceImpl::DidDeleteID,
                   weak_factory_.GetWeakPtr(), app_id, was_subscribed));

  } else {
    auto unregister_callback =
        base::BindOnce(&PushMessagingServiceImpl::DidUnregister,
                       weak_factory_.GetWeakPtr(), was_subscribed);
#if defined(OS_ANDROID)
    // On Android the backend is different, and requires the original sender_id.
    // UnsubscribeBecausePermissionRevoked and
    // DidDeleteServiceWorkerRegistration sometimes call us with an empty one.
    if (sender_id.empty()) {
      std::move(unregister_callback).Run(gcm::GCMClient::INVALID_PARAMETER);
    } else {
      GetGCMDriver()->UnregisterWithSenderId(app_id,
                                             NormalizeSenderInfo(sender_id),
                                             std::move(unregister_callback));
    }
#else
    GetGCMDriver()->Unregister(app_id, std::move(unregister_callback));
#endif
  }
}

void PushMessagingServiceImpl::DidUnregister(bool was_subscribed,
                                             gcm::GCMClient::Result result) {
  RecordUnsubscribeGCMResult(result);
  DidUnsubscribe(std::string() /* app_id_when_instance_id */, was_subscribed);
}

void PushMessagingServiceImpl::DidDeleteID(const std::string& app_id,
                                           bool was_subscribed,
                                           InstanceID::Result result) {
  RecordUnsubscribeIIDResult(result);
  // DidUnsubscribe must be run asynchronously when passing a non-empty
  // |app_id_when_instance_id|, since it calls
  // InstanceIDDriver::RemoveInstanceID which deletes the InstanceID itself.
  // Calling that immediately would cause a use-after-free in our caller.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PushMessagingServiceImpl::DidUnsubscribe,
                     weak_factory_.GetWeakPtr(), app_id, was_subscribed));
}

void PushMessagingServiceImpl::DidUnsubscribe(
    const std::string& app_id_when_instance_id,
    bool was_subscribed) {
  if (!app_id_when_instance_id.empty())
    GetInstanceIDDriver()->RemoveInstanceID(app_id_when_instance_id);

  if (was_subscribed)
    DecreasePushSubscriptionCount(1, false /* was_pending */);

  if (!unsubscribe_callback_for_testing_.is_null())
    unsubscribe_callback_for_testing_.Run();
}

void PushMessagingServiceImpl::SetUnsubscribeCallbackForTesting(
    const base::Closure& callback) {
  unsubscribe_callback_for_testing_ = callback;
}

// DidDeleteServiceWorkerRegistration methods ----------------------------------

void PushMessagingServiceImpl::DidDeleteServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  const PushMessagingAppIdentifier& app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          profile_, origin, service_worker_registration_id);
  if (app_identifier.is_null()) {
    if (!service_worker_unregistered_callback_for_testing_.is_null())
      service_worker_unregistered_callback_for_testing_.Run();
    return;
  }
  // Note this will not fully unsubscribe pre-InstanceID subscriptions on
  // Android from GCM, as that requires a sender_id. (Ideally we'd fetch it
  // from the SWDB in some "before_unregistered" SWObserver event.)
  UnsubscribeInternal(
      blink::mojom::PushUnregistrationReason::SERVICE_WORKER_UNREGISTERED,
      origin, service_worker_registration_id, app_identifier.app_id(),
      std::string() /* sender_id */,
      base::Bind(&UnregisterCallbackToClosure,
                 service_worker_unregistered_callback_for_testing_.is_null()
                     ? base::DoNothing()
                     : service_worker_unregistered_callback_for_testing_));
}

void PushMessagingServiceImpl::SetServiceWorkerUnregisteredCallbackForTesting(
    const base::Closure& callback) {
  service_worker_unregistered_callback_for_testing_ = callback;
}

// DidDeleteServiceWorkerDatabase methods --------------------------------------

void PushMessagingServiceImpl::DidDeleteServiceWorkerDatabase() {
  std::vector<PushMessagingAppIdentifier> app_identifiers =
      PushMessagingAppIdentifier::GetAll(profile_);

  base::RepeatingClosure completed_closure = base::BarrierClosure(
      app_identifiers.size(),
      service_worker_database_wiped_callback_for_testing_.is_null()
          ? base::DoNothing()
          : service_worker_database_wiped_callback_for_testing_);

  for (const PushMessagingAppIdentifier& app_identifier : app_identifiers) {
    // Note this will not fully unsubscribe pre-InstanceID subscriptions on
    // Android from GCM, as that requires a sender_id. We can't fetch those from
    // the Service Worker database anymore as it's been deleted.
    UnsubscribeInternal(
        blink::mojom::PushUnregistrationReason::SERVICE_WORKER_DATABASE_WIPED,
        app_identifier.origin(),
        app_identifier.service_worker_registration_id(),
        app_identifier.app_id(), std::string() /* sender_id */,
        base::Bind(&UnregisterCallbackToClosure, completed_closure));
  }
}

void PushMessagingServiceImpl::SetServiceWorkerDatabaseWipedCallbackForTesting(
    const base::Closure& callback) {
  service_worker_database_wiped_callback_for_testing_ = callback;
}

// OnContentSettingChanged methods ---------------------------------------------

void PushMessagingServiceImpl::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (content_type != ContentSettingsType::NOTIFICATIONS)
    return;

  std::vector<PushMessagingAppIdentifier> all_app_identifiers =
      PushMessagingAppIdentifier::GetAll(profile_);

  base::Closure barrier_closure = base::BarrierClosure(
      all_app_identifiers.size(),
      content_setting_changed_callback_for_testing_.is_null()
          ? base::DoNothing()
          : content_setting_changed_callback_for_testing_);

  for (const PushMessagingAppIdentifier& app_identifier : all_app_identifiers) {
    // If |primary_pattern| is not valid, we should always check for a
    // permission change because it can happen for example when the entire
    // Push or Notifications permissions are cleared.
    // Otherwise, the permission should be checked if the pattern matches the
    // origin.
    if (primary_pattern.IsValid() &&
        !primary_pattern.Matches(app_identifier.origin())) {
      barrier_closure.Run();
      continue;
    }

    if (IsPermissionSet(app_identifier.origin())) {
      barrier_closure.Run();
      continue;
    }

    bool need_sender_id = false;
#if defined(OS_ANDROID)
    need_sender_id =
        !PushMessagingAppIdentifier::UseInstanceID(app_identifier.app_id());
#endif
    if (need_sender_id) {
      GetSenderId(
          profile_, app_identifier.origin(),
          app_identifier.service_worker_registration_id(),
          base::Bind(
              &PushMessagingServiceImpl::UnsubscribeBecausePermissionRevoked,
              weak_factory_.GetWeakPtr(), app_identifier,
              base::Bind(&UnregisterCallbackToClosure, barrier_closure)));
    } else {
      UnsubscribeInternal(
          blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED,
          app_identifier.origin(),
          app_identifier.service_worker_registration_id(),
          app_identifier.app_id(), std::string() /* sender_id */,
          base::Bind(&UnregisterCallbackToClosure, barrier_closure));
    }
  }
}

void PushMessagingServiceImpl::UnsubscribeBecausePermissionRevoked(
    const PushMessagingAppIdentifier& app_identifier,
    UnregisterCallback callback,
    const std::string& sender_id,
    bool success,
    bool not_found) {
  // Unsubscribe the PushMessagingAppIdentifier with the push service.
  // It's possible for GetSenderId to have failed and sender_id to be empty, if
  // cookies (and the SW database) for an origin got cleared before permissions
  // are cleared for the origin. In that case for legacy GCM registrations on
  // Android, Unsubscribe will just delete the app identifier to block future
  // messages.
  // TODO(johnme): Auto-unregister before SW DB is cleared (crbug.com/402458).
  UnsubscribeInternal(
      blink::mojom::PushUnregistrationReason::PERMISSION_REVOKED,
      app_identifier.origin(), app_identifier.service_worker_registration_id(),
      app_identifier.app_id(), sender_id, std::move(callback));
}

void PushMessagingServiceImpl::SetContentSettingChangedCallbackForTesting(
    const base::Closure& callback) {
  content_setting_changed_callback_for_testing_ = callback;
}

// KeyedService methods -------------------------------------------------------

void PushMessagingServiceImpl::Shutdown() {
  GetGCMDriver()->RemoveAppHandler(kPushMessagingAppIdentifierPrefix);
  HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(this);
}

// content::NotificationObserver methods ---------------------------------------

void PushMessagingServiceImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  shutdown_started_ = true;
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  in_flight_keep_alive_.reset();
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
}

// Helper methods --------------------------------------------------------------

std::string PushMessagingServiceImpl::NormalizeSenderInfo(
    const std::string& application_server_key) const {
  // Only encode the |application_server_key| when it is a NIST P-256 public key
  // in uncompressed format, verified through its length and the 0x04 prefix
  // byte.
  if (application_server_key.size() != 65 || application_server_key[0] != 0x04)
    return application_server_key;

  std::string encoded_application_server_key;
  base::Base64UrlEncode(application_server_key,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_application_server_key);

  return encoded_application_server_key;
}

// Assumes user_visible always since this is just meant to check
// if the permission was previously granted and not revoked.
bool PushMessagingServiceImpl::IsPermissionSet(const GURL& origin) {
  return GetPermissionStatus(origin, true /* user_visible */) ==
         blink::mojom::PermissionStatus::GRANTED;
}

void PushMessagingServiceImpl::GetEncryptionInfoForAppId(
    const std::string& app_id,
    const std::string& sender_id,
    gcm::GCMEncryptionProvider::EncryptionInfoCallback callback) {
  if (PushMessagingAppIdentifier::UseInstanceID(app_id)) {
    GetInstanceIDDriver()->GetInstanceID(app_id)->GetEncryptionInfo(
        NormalizeSenderInfo(sender_id), std::move(callback));
  } else {
    GetGCMDriver()->GetEncryptionInfo(app_id, std::move(callback));
  }
}

GURL PushMessagingServiceImpl::CreateEndpoint(
    const std::string& subscription_id) const {
  const GURL endpoint(kPushMessagingGcmEndpoint + subscription_id);
  DCHECK(endpoint.is_valid());
  return endpoint;
}

gcm::GCMDriver* PushMessagingServiceImpl::GetGCMDriver() const {
  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile_);
  CHECK(gcm_profile_service);
  CHECK(gcm_profile_service->driver());
  return gcm_profile_service->driver();
}

instance_id::InstanceIDDriver* PushMessagingServiceImpl::GetInstanceIDDriver()
    const {
  instance_id::InstanceIDProfileService* instance_id_profile_service =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile_);
  CHECK(instance_id_profile_service);
  CHECK(instance_id_profile_service->driver());
  return instance_id_profile_service->driver();
}

content::DevToolsBackgroundServicesContext*
PushMessagingServiceImpl::GetDevToolsContext(const GURL& origin) const {
  auto* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(profile_, origin);
  if (!storage_partition)
    return nullptr;

  auto* devtools_context =
      storage_partition->GetDevToolsBackgroundServicesContext();

  if (!devtools_context->IsRecording(
          content::DevToolsBackgroundService::kPushMessaging)) {
    return nullptr;
  }

  return devtools_context;
}
