// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_IMPL_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_IMPL_H_

#include <stdint.h>
#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/abusive_origin_permission_revocation_request.h"
#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"
#include "chrome/browser/push_messaging/push_messaging_refresher.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/crypto/gcm_encryption_provider.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/push_messaging_service.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-forward.h"

class GURL;
class Profile;
class PushMessagingAppIdentifier;
class PushMessagingServiceTest;
class ScopedKeepAlive;

namespace blink {
namespace mojom {
enum class PushEventStatus;
enum class PushRegistrationStatus;
}  // namespace mojom
}  // namespace blink

namespace content {
class DevToolsBackgroundServicesContext;
}  // namespace content

namespace gcm {
class GCMDriver;
}  // namespace gcm

namespace instance_id {
class InstanceIDDriver;
}  // namespace instance_id

namespace {
struct PendingMessage {
  PendingMessage(std::string app_id, gcm::IncomingMessage message);
  PendingMessage(PendingMessage&& other);
  ~PendingMessage();

  PendingMessage& operator=(PendingMessage&& other);

  std::string app_id;
  gcm::IncomingMessage message;
};
}  // namespace

class PushMessagingServiceImpl : public content::PushMessagingService,
                                 public gcm::GCMAppHandler,
                                 public content_settings::Observer,
                                 public KeyedService,
                                 public content::NotificationObserver,
                                 public PushMessagingRefresher::Observer {
 public:
  // If any Service Workers are using push, starts GCM and adds an app handler.
  static void InitializeForProfile(Profile* profile);

  explicit PushMessagingServiceImpl(Profile* profile);
  ~PushMessagingServiceImpl() override;

  // Check and remove subscriptions that are expired when |this| is initialized
  void RemoveExpiredSubscriptions();

  // Gets the permission status for the given |origin|.
  blink::mojom::PermissionStatus GetPermissionStatus(const GURL& origin,
                                                     bool user_visible);

  // gcm::GCMAppHandler implementation.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  void OnMessageDecryptionFailed(const std::string& app_id,
                                 const std::string& message_id,
                                 const std::string& error_message) override;
  bool CanHandle(const std::string& app_id) const override;

  // content::PushMessagingService implementation:
  void SubscribeFromDocument(const GURL& requesting_origin,
                             int64_t service_worker_registration_id,
                             int render_process_id,
                             int render_frame_id,
                             blink::mojom::PushSubscriptionOptionsPtr options,
                             bool user_gesture,
                             RegisterCallback callback) override;
  void SubscribeFromWorker(const GURL& requesting_origin,
                           int64_t service_worker_registration_id,
                           blink::mojom::PushSubscriptionOptionsPtr options,
                           RegisterCallback callback) override;
  void GetSubscriptionInfo(const GURL& origin,
                           int64_t service_worker_registration_id,
                           const std::string& sender_id,
                           const std::string& subscription_id,
                           SubscriptionInfoCallback callback) override;
  void Unsubscribe(blink::mojom::PushUnregistrationReason reason,
                   const GURL& requesting_origin,
                   int64_t service_worker_registration_id,
                   const std::string& sender_id,
                   UnregisterCallback) override;
  bool SupportNonVisibleMessages() override;
  void DidDeleteServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id) override;
  void DidDeleteServiceWorkerDatabase() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // Fires the `pushsubscriptionchange` event to the associated service worker
  // of |app_identifier|, which is the app identifier for |old_subscription|
  // whereas |new_subscription| can be either null e.g. when a subscription is
  // lost due to permission changes or a new subscription when it was refreshed.
  void FirePushSubscriptionChange(
      const PushMessagingAppIdentifier& app_identifier,
      base::OnceClosure completed_closure,
      blink::mojom::PushSubscriptionPtr new_subscription,
      blink::mojom::PushSubscriptionPtr old_subscription);

  // KeyedService implementation.
  void Shutdown() override;

  // content::NotificationObserver implementation
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // WARNING: Only call this function if features::kPushSubscriptionChangeEvent
  // is enabled, will be later used by the Push Service to trigger subscription
  // refreshes
  void OnSubscriptionInvalidation(const std::string& app_id);

  // PushMessagingRefresher::Observer implementation
  // Initiate unsubscribe task when old subscription becomes invalid
  void OnOldSubscriptionExpired(const std::string& app_id,
                                const std::string& sender_id) override;
  void OnRefreshFinished(
      const PushMessagingAppIdentifier& app_identifier) override;

  void SetMessageCallbackForTesting(const base::Closure& callback);
  void SetUnsubscribeCallbackForTesting(const base::Closure& callback);
  void SetInvalidationCallbackForTesting(base::OnceClosure callback);
  void SetContentSettingChangedCallbackForTesting(
      base::RepeatingClosure callback);
  void SetServiceWorkerUnregisteredCallbackForTesting(
      base::RepeatingClosure callback);
  void SetServiceWorkerDatabaseWipedCallbackForTesting(
      base::RepeatingClosure callback);
  void SetRemoveExpiredSubscriptionsCallbackForTesting(
      base::OnceClosure closure);

 private:
  friend class PushMessagingBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PushMessagingServiceTest, NormalizeSenderInfo);
  FRIEND_TEST_ALL_PREFIXES(PushMessagingServiceTest, PayloadEncryptionTest);

  // A subscription is pending until it has succeeded or failed.
  void IncreasePushSubscriptionCount(int add, bool is_pending);
  void DecreasePushSubscriptionCount(int subtract, bool was_pending);

  // OnMessage methods ---------------------------------------------------------

  void DeliverMessageCallback(const std::string& app_id,
                              const GURL& requesting_origin,
                              int64_t service_worker_registration_id,
                              const gcm::IncomingMessage& message,
                              base::OnceClosure message_handled_closure,
                              blink::mojom::PushEventStatus status);

  void DidHandleMessage(const std::string& app_id,
                        const std::string& push_message_id,
                        base::OnceClosure completion_closure,
                        bool did_show_generic_notification);

  void OnCheckedOriginForAbuse(
      const std::string& app_id,
      const gcm::IncomingMessage& message,
      AbusiveOriginPermissionRevocationRequest::Outcome outcome);

  void CheckOriginForAbuseAndDispatchNextMessage();

  base::OnceClosure message_handled_callback() {
    return message_callback_for_testing_.is_null()
               ? base::DoNothing()
               : message_callback_for_testing_;
  }

  // Subscribe methods ---------------------------------------------------------

  void DoSubscribe(PushMessagingAppIdentifier app_identifier,
                   blink::mojom::PushSubscriptionOptionsPtr options,
                   RegisterCallback callback,
                   int render_process_id,
                   int render_frame_id,
                   ContentSetting permission_status);

  void SubscribeEnd(RegisterCallback callback,
                    const std::string& subscription_id,
                    const GURL& endpoint,
                    const base::Optional<base::Time>& expiration_time,
                    const std::vector<uint8_t>& p256dh,
                    const std::vector<uint8_t>& auth,
                    blink::mojom::PushRegistrationStatus status);

  void SubscribeEndWithError(RegisterCallback callback,
                             blink::mojom::PushRegistrationStatus status);

  void DidSubscribe(const PushMessagingAppIdentifier& app_identifier,
                    const std::string& sender_id,
                    RegisterCallback callback,
                    const std::string& subscription_id,
                    instance_id::InstanceID::Result result);

  void DidSubscribeWithEncryptionInfo(
      const PushMessagingAppIdentifier& app_identifier,
      RegisterCallback callback,
      const std::string& subscription_id,
      const GURL& endpoint,
      std::string p256dh,
      std::string auth_secret);

  // GetSubscriptionInfo methods -----------------------------------------------

  void DidValidateSubscription(
      const std::string& app_id,
      const std::string& sender_id,
      const GURL& endpoint,
      const base::Optional<base::Time>& expiration_time,
      SubscriptionInfoCallback callback,
      bool is_valid);

  void DidGetEncryptionInfo(const GURL& endpoint,
                            const base::Optional<base::Time>& expiration_time,
                            SubscriptionInfoCallback callback,
                            std::string p256dh,
                            std::string auth_secret) const;

  // Unsubscribe methods -------------------------------------------------------

  // |origin|, |service_worker_registration_id| and |app_id| should be provided
  // whenever they can be obtained. It's valid for |origin| to be empty and
  // |service_worker_registration_id| to be kInvalidServiceWorkerRegistrationId,
  // or for app_id to be empty, but not both at once.
  void UnsubscribeInternal(blink::mojom::PushUnregistrationReason reason,
                           const GURL& origin,
                           int64_t service_worker_registration_id,
                           const std::string& app_id,
                           const std::string& sender_id,
                           UnregisterCallback callback);

  void DidClearPushSubscriptionId(blink::mojom::PushUnregistrationReason reason,
                                  const std::string& app_id,
                                  const std::string& sender_id,
                                  UnregisterCallback callback);

  void DidUnregister(bool was_subscribed, gcm::GCMClient::Result result);
  void DidDeleteID(const std::string& app_id,
                   bool was_subscribed,
                   instance_id::InstanceID::Result result);
  void DidUnsubscribe(const std::string& app_id_when_instance_id,
                      bool was_subscribed);

  // OnContentSettingChanged methods -------------------------------------------

  void GetPushSubscriptionFromAppIdentifier(
      const PushMessagingAppIdentifier& app_identifier,
      base::OnceCallback<void(blink::mojom::PushSubscriptionPtr)> callback);

  void DidGetSWData(
      const PushMessagingAppIdentifier& app_identifier,
      base::OnceCallback<void(blink::mojom::PushSubscriptionPtr)> callback,
      const std::string& sender_id,
      const std::string& subscription_id);

  void GetPushSubscriptionFromAppIdentifierEnd(
      base::OnceCallback<void(blink::mojom::PushSubscriptionPtr)> callback,
      const std::string& sender_id,
      bool is_valid,
      const GURL& endpoint,
      const base::Optional<base::Time>& expiration_time,
      const std::vector<uint8_t>& p256dh,
      const std::vector<uint8_t>& auth);

  // OnSubscriptionInvalidation methods-----------------------------------------

  void GetOldSubscription(PushMessagingAppIdentifier old_app_identifier,
                          const std::string& sender_id);

  // After gathering all relavent information to start the refresh,
  // generate a new app id and initiate refresh
  void StartRefresh(PushMessagingAppIdentifier old_app_identifier,
                    const std::string& sender_id,
                    blink::mojom::PushSubscriptionPtr old_subscription);

  // Makes a new susbcription and replaces the old subscription by new
  // subscription in preferences and service worker database
  void UpdateSubscription(PushMessagingAppIdentifier app_identifier,
                          blink::mojom::PushSubscriptionOptionsPtr options,
                          RegisterCallback callback);

  // After the subscription is updated, fire a `pushsubscriptionchange` event
  // and notify the |refresher_|
  void DidUpdateSubscription(const std::string& new_app_id,
                             const std::string& old_app_id,
                             blink::mojom::PushSubscriptionPtr old_subscription,
                             const std::string& sender_id,
                             const std::string& registration_id,
                             const GURL& endpoint,
                             const base::Optional<base::Time>& expiration_time,
                             const std::vector<uint8_t>& p256dh,
                             const std::vector<uint8_t>& auth,
                             blink::mojom::PushRegistrationStatus status);
  // Helper methods ------------------------------------------------------------

  // The subscription given in |identifier| will be unsubscribed (and a
  // `pushsubscriptionchange` event fires if
  // features::kPushSubscriptionChangeEvent is enabled)
  void UnexpectedChange(PushMessagingAppIdentifier identifier,
                        blink::mojom::PushUnregistrationReason reason,
                        base::OnceClosure completed_closure);

  void UnexpectedUnsubscribe(const PushMessagingAppIdentifier& app_identifier,
                             blink::mojom::PushUnregistrationReason reason,
                             UnregisterCallback unregister_callback);

  void DidGetSenderIdUnexpectedUnsubscribe(
      const PushMessagingAppIdentifier& app_identifier,
      blink::mojom::PushUnregistrationReason reason,
      UnregisterCallback callback,
      const std::string& sender_id);

  void FirePushSubscriptionChangeCallback(
      const PushMessagingAppIdentifier& app_identifier,
      blink::mojom::PushEventStatus status);

  // Checks if a given origin is allowed to use Push.
  bool IsPermissionSet(const GURL& origin, bool user_visible = true);

  // Wrapper around {GCMDriver, InstanceID}::GetEncryptionInfo.
  void GetEncryptionInfoForAppId(
      const std::string& app_id,
      const std::string& sender_id,
      gcm::GCMEncryptionProvider::EncryptionInfoCallback callback);

  gcm::GCMDriver* GetGCMDriver() const;

  instance_id::InstanceIDDriver* GetInstanceIDDriver() const;

  content::DevToolsBackgroundServicesContext* GetDevToolsContext(
      const GURL& origin) const;

  // Testing methods -----------------------------------------------------------

  // Callback to be invoked when a message has been dispatched. Enables tests to
  // observe message delivery before it's dispatched to the Service Worker.
  using MessageDispatchedCallback =
      base::Callback<void(const std::string& app_id,
                          const GURL& origin,
                          int64_t service_worker_registration_id,
                          base::Optional<std::string> payload)>;

  void SetMessageDispatchedCallbackForTesting(
      const MessageDispatchedCallback& callback) {
    message_dispatched_callback_for_testing_ = callback;
  }

  Profile* profile_;
  std::unique_ptr<AbusiveOriginPermissionRevocationRequest>
      abusive_origin_revocation_request_;
  std::queue<PendingMessage> messages_pending_permission_check_;

  int push_subscription_count_;
  int pending_push_subscription_count_;

  base::Closure message_callback_for_testing_;
  base::Closure unsubscribe_callback_for_testing_;
  base::Closure content_setting_changed_callback_for_testing_;
  base::Closure service_worker_unregistered_callback_for_testing_;
  base::Closure service_worker_database_wiped_callback_for_testing_;
  base::OnceClosure remove_expired_subscriptions_callback_for_testing_;
  base::OnceClosure invalidation_callback_for_testing_;

  PushMessagingNotificationManager notification_manager_;

  PushMessagingRefresher refresher_;

  ScopedObserver<PushMessagingRefresher, PushMessagingRefresher::Observer>
      refresh_observer_{this};
  // A multiset containing one entry for each in-flight push message delivery,
  // keyed by the receiver's app id.
  std::multiset<std::string> in_flight_message_deliveries_;

  MessageDispatchedCallback message_dispatched_callback_for_testing_;

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  // KeepAlive registered while we have in-flight push messages, to make sure
  // we can finish processing them without being interrupted.
  std::unique_ptr<ScopedKeepAlive> in_flight_keep_alive_;
#endif

  content::NotificationRegistrar registrar_;

  // True when shutdown has started. Do not allow processing of incoming
  // messages when this is true.
  bool shutdown_started_ = false;

  base::WeakPtrFactory<PushMessagingServiceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PushMessagingServiceImpl);
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_IMPL_H_
