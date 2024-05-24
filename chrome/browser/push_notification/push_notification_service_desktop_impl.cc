// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_service_desktop_impl.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/push_notification/metrics/push_notification_metrics.h"
#include "chrome/browser/push_notification/prefs/push_notification_prefs.h"
#include "chrome/browser/push_notification/protos/notifications_multi_login_update.pb.h"
#include "chrome/browser/push_notification/server_client/push_notification_desktop_api_call_flow_impl.h"
#include "chrome/browser/push_notification/server_client/push_notification_server_client.h"
#include "chrome/browser/push_notification/server_client/push_notification_server_client_desktop_impl.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const char kPushNotificationAppId[] = "com.google.chrome.push_notification";
const char kPushNotificationScope[] = "GCM";
const char kPushNotificationSenderId[] = "745476177629";
const char kClientId[] = "ChromeDesktop";

}  // namespace

namespace push_notification {

PushNotificationServiceDesktopImpl::PushNotificationServiceDesktopImpl(
    PrefService* pref_service,
    instance_id::InstanceIDDriver* instance_id_driver,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service),
      instance_id_driver_(instance_id_driver),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {
  CHECK(pref_service_);
  CHECK(instance_id_driver_);
  CHECK(identity_manager_);
  CHECK(url_loader_factory_);
  initialization_on_demand_scheduler_ =
      ash::nearby::NearbySchedulerFactory::CreateOnDemandScheduler(
          /*retry_failures=*/true, /*require_connectivity=*/true,
          prefs::kPushNotificationRegistrationAttemptBackoffSchedulerPrefName,
          pref_service_,
          base::BindRepeating(&PushNotificationServiceDesktopImpl::Initialize,
                              base::Unretained(this)),
          Feature::NEARBY_INFRA, base::DefaultClock::GetInstance());
  initialization_on_demand_scheduler_->Start();
}

PushNotificationServiceDesktopImpl::~PushNotificationServiceDesktopImpl() =
    default;

void PushNotificationServiceDesktopImpl::ShutdownHandler() {
  // Shutdown() should come before and it removes us from the list of app
  // handlers of gcm::GCMDriver so this shouldn't ever been called.
  NOTREACHED_IN_MIGRATION()
      << "The Push Notification Service should have removed itself "
         "from the list of app handlers before this could be called.";
}

void PushNotificationServiceDesktopImpl::OnStoreReset() {
  // Reset prefs.
  pref_service_->SetString(
      prefs::kPushNotificationRepresentativeTargetIdPrefName, std::string());
}

void PushNotificationServiceDesktopImpl::OnMessage(
    const std::string& app_id,
    const gcm::IncomingMessage& message) {
  PushNotificationClientManager::PushNotificationMessage
      push_notification_message;
  push_notification_message.sender_id = message.sender_id;
  push_notification_message.message_id = message.message_id;
  push_notification_message.collapse_key = message.collapse_key;
  push_notification_message.raw_data = message.raw_data;

  for (const auto& data : message.data) {
    push_notification_message.data.insert_or_assign(data.first, data.second);
  }

  client_manager_->NotifyPushNotificationClientOfMessage(
      push_notification_message);
}

void PushNotificationServiceDesktopImpl::OnMessagesDeleted(
    const std::string& app_id) {}
void PushNotificationServiceDesktopImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  NOTREACHED_IN_MIGRATION()
      << "The Push Notification Service shouldn't have sent messages upstream";
}
void PushNotificationServiceDesktopImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED_IN_MIGRATION()
      << "The Push Notification Service shouldn't have sent messages upstream";
}

// Intentional no-op. We don't support encryption/decryption of messages.
void PushNotificationServiceDesktopImpl::OnMessageDecryptionFailed(
    const std::string& app_id,
    const std::string& message_id,
    const std::string& error_message) {}

// PushNotificationService does not support messages from any other app.
bool PushNotificationServiceDesktopImpl::CanHandle(
    const std::string& app_id) const {
  return false;
}

void PushNotificationServiceDesktopImpl::Shutdown() {
  client_manager_.reset();
  token_.clear();
  instance_id_driver_->GetInstanceID(kPushNotificationAppId)
      ->gcm_driver()
      ->RemoveAppHandler(kPushNotificationAppId);
}

void PushNotificationServiceDesktopImpl::Initialize() {
  if (is_initialized_) {
    return;
  }

  instance_id_driver_->GetInstanceID(kPushNotificationAppId)
      ->GetToken(
          kPushNotificationSenderId, kPushNotificationScope,
          /*time_to_live=*/base::TimeDelta(), /*flags=*/{},
          base::BindOnce(&PushNotificationServiceDesktopImpl::OnTokenReceived,
                         base::Unretained(this), /*token_request_start_time=*/
                         base::TimeTicks::Now()));
}

void PushNotificationServiceDesktopImpl::OnTokenReceived(
    base::TimeTicks token_request_start_time,
    const std::string& token,
    instance_id::InstanceID::Result result) {
  if (result != instance_id::InstanceID::Result::SUCCESS) {
    LOG(ERROR) << "Failed to retrieve GCM token: " << result;
    metrics::RecordPushNotificationGcmTokenRetrievalResult(/*success=*/false);
    initialization_on_demand_scheduler_->HandleResult(/*success=*/false);
    return;
  }

  metrics::RecordPushNotificationGcmTokenRetrievalResult(/*success=*/true);
  metrics::RecordPushNotificationServiceTimeToRetrieveToken(
      /*total_retrieval_time=*/base::TimeTicks::Now() -
      token_request_start_time);
  VLOG(1) << "Successfully retrieved GCM token. ";
  token_ = token;

  // Add `PushNotificationService` as a GCM app handler.
  instance_id_driver_->GetInstanceID(kPushNotificationAppId)
      ->gcm_driver()
      ->AddAppHandler(kPushNotificationAppId, this);

  std::string representative_target_id = pref_service_->GetString(
      prefs::kPushNotificationRepresentativeTargetIdPrefName);

  // Create the `NotificationsMultiLoginUpdateRequest` proto which is used to
  // make the registration API call.
  push_notification::proto::NotificationsMultiLoginUpdateRequest request_proto;
  request_proto.mutable_target()->set_channel_type(
      push_notification::proto::ChannelType::GCM_DEVICE_PUSH);
  request_proto.mutable_target()
      ->mutable_delivery_address()
      ->mutable_gcm_device_address()
      ->set_registration_id(token_);
  request_proto.mutable_target()
      ->mutable_delivery_address()
      ->mutable_gcm_device_address()
      ->set_application_id(kPushNotificationAppId);

  // `representative_target_id` is left empty the first time we register with
  // the Push Notification Service. It is then returned to us in the response
  // proto and stored in prefs. When we have a stored representative target id,
  // we use it to help the Push Notification Service stablize the target across
  // registrations if the GCM registration token changes.
  if (!representative_target_id.empty()) {
    request_proto.mutable_target()->set_representative_target_id(
        representative_target_id);
  }
  request_proto.add_registrations();
  request_proto.set_registration_reason(
      push_notification::proto::RegistrationReason::COLLABORATOR_API_CALL);
  request_proto.set_client_id(kClientId);

  // Construct a HTTP client for the request. The HTTP client lifetime is
  // tied to a single request.
  server_client_ = PushNotificationServerClientDesktopImpl::Factory::Create(
      std::make_unique<PushNotificationDesktopApiCallFlowImpl>(),
      identity_manager_, url_loader_factory_);

  server_client_->RegisterWithPushNotificationService(
      request_proto,
      base::BindOnce(&PushNotificationServiceDesktopImpl::
                         OnPushNotificationRegistrationSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*api_call_start_time=*/base::TimeTicks::Now()),
      base::BindOnce(&PushNotificationServiceDesktopImpl::
                         OnPushNotificationRegistrationFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*api_call_start_time=*/base::TimeTicks::Now()));
}

void PushNotificationServiceDesktopImpl::OnPushNotificationRegistrationSuccess(
    base::TimeTicks api_call_start_time,
    const proto::NotificationsMultiLoginUpdateResponse& response) {
  metrics::
      RecordPushNotificationServiceTimeToReceiveRegistrationSuccessResponse(
          /*registration_response_time=*/base::TimeTicks::Now() -
          api_call_start_time);
  metrics::RecordPushNotificationServiceRegistrationResult(/*success=*/true);
  VLOG(1) << __func__ << ": Push notification service registration successful";
  is_initialized_ = true;
  server_client_.reset();
  initialization_on_demand_scheduler_->HandleResult(/*success=*/true);
  CHECK(response.registration_results_size() == 1);
  pref_service_->SetString(
      prefs::kPushNotificationRepresentativeTargetIdPrefName,
      response.registration_results(0).target().representative_target_id());
}

void PushNotificationServiceDesktopImpl::OnPushNotificationRegistrationFailure(
    base::TimeTicks api_call_start_time,
    PushNotificationDesktopApiCallFlow::PushNotificationApiCallFlowError
        error) {
  metrics::
      RecordPushNotificationServiceTimeToReceiveRegistrationFailureResponse(
          /*registration_response_time=*/base::TimeTicks::Now() -
          api_call_start_time);
  metrics::RecordPushNotificationServiceRegistrationResult(/*success=*/false);
  LOG(ERROR) << __func__
             << ": Push notification service registration failure: " << error;
  server_client_.reset();

  // Remove ourselves as a GCM app handler since initialization failed.
  instance_id_driver_->GetInstanceID(kPushNotificationAppId)
      ->gcm_driver()
      ->RemoveAppHandler(kPushNotificationAppId);

  initialization_on_demand_scheduler_->HandleResult(/*success=*/false);
}

}  // namespace push_notification
