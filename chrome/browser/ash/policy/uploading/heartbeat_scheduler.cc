// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/uploading/heartbeat_scheduler.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/common/chrome_switches.h"
#include "components/gcm_driver/gcm_driver.h"

namespace policy {

namespace {

constexpr base::TimeDelta kMinHeartbeatInterval = base::Seconds(30);
constexpr base::TimeDelta kMaxHeartbeatInterval = base::Days(1);

// Our sender ID we send up with all of our GCM messages.
const char kHeartbeatGCMAppID[] = "com.google.chromeos.monitoring";

// The default destination we send our GCM messages to.
const char kHeartbeatGCMDestinationID[] = "1013309121859";
const char kHeartbeatGCMSenderSuffix[] = "@google.com";

// Destination of upstream notification sign up message.
const char kUpstreamNotificationSignUpDestinationID[] =
    "https://gcm.googleapis.com/gcm/gcm.event_tracker";

// A bit mask, listening events of upstream notification.
const char kUpstreamNotificationSignUpListeningEvents[] =
    "7";  // START | DISCONNECTED | HEARTBEAT

const char kGcmMessageTypeKey[] = "type";
const char kHeartbeatTimestampKey[] = "timestamp";
const char kHeartbeatCustomerIdKey[] = "customer_id";
const char kHeartbeatDeviceIDKey[] = "device_id";
const char kHeartbeatTypeValue[] = "hb";
const char kUpstreamNotificationNotifyKey[] = "notify";
const char kUpstreamNotificationRegIdKey[] = "registration_id";

// If we get an error registering with GCM, try again in two minutes.
constexpr base::TimeDelta kRegistrationRetryDelay = base::Minutes(2);

const char kHeartbeatSchedulerScope[] =
    "policy.heartbeat_scheduler.upstream_notification";

// Returns the destination ID for GCM heartbeats.
std::string GetDestinationID() {
  std::string receiver_id = kHeartbeatGCMDestinationID;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMonitoringDestinationID)) {
    receiver_id = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kMonitoringDestinationID);
  }
  return receiver_id;
}

}  // namespace

// static
const base::TimeDelta HeartbeatScheduler::kDefaultHeartbeatInterval =
    base::Minutes(2);

// Helper class used to manage GCM registration (handles retrying after
// errors, etc).
class HeartbeatRegistrationHelper {
 public:
  using RegistrationHelperCallback =
      base::OnceCallback<void(const std::string& registration_id)>;

  HeartbeatRegistrationHelper(
      gcm::GCMDriver* gcm_driver,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  HeartbeatRegistrationHelper(const HeartbeatRegistrationHelper&) = delete;
  HeartbeatRegistrationHelper& operator=(const HeartbeatRegistrationHelper&) =
      delete;

  void Register(RegistrationHelperCallback callback);

 private:
  void AttemptRegistration();

  // Callback invoked once a registration attempt has finished.
  void OnRegisterAttemptComplete(const std::string& registration_id,
                                 gcm::GCMClient::Result result);

  // GCMDriver to use to register.
  const raw_ptr<gcm::GCMDriver> gcm_driver_;

  // Callback to invoke when we have completed GCM registration.
  RegistrationHelperCallback callback_;

  // TaskRunner used for scheduling retry attempts.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Should remain the last member so it will be destroyed first and
  // invalidate all weak pointers.
  base::WeakPtrFactory<HeartbeatRegistrationHelper> weak_factory_{this};
};

HeartbeatRegistrationHelper::HeartbeatRegistrationHelper(
    gcm::GCMDriver* gcm_driver,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : gcm_driver_(gcm_driver), task_runner_(task_runner) {}

void HeartbeatRegistrationHelper::Register(
    RegistrationHelperCallback callback) {
  // Should only call Register() once.
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);
  AttemptRegistration();
}

void HeartbeatRegistrationHelper::AttemptRegistration() {
  std::vector<std::string> destinations;
  destinations.push_back(GetDestinationID());
  gcm_driver_->Register(
      kHeartbeatGCMAppID, destinations,
      base::BindOnce(&HeartbeatRegistrationHelper::OnRegisterAttemptComplete,
                     weak_factory_.GetWeakPtr()));
}

void HeartbeatRegistrationHelper::OnRegisterAttemptComplete(
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  DVLOG(1) << "Received Register() response: " << result;
  // TODO(atwilson): Track GCM errors via UMA (http://crbug.com/459238).
  switch (result) {
    case gcm::GCMClient::SUCCESS:
      std::move(callback_).Run(registration_id);
      // This helper may be freed now, so do not access any member variables
      // after this point.
      return;

    case gcm::GCMClient::NETWORK_ERROR:
    case gcm::GCMClient::SERVER_ERROR:
      // Transient error - try again after a delay.
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&HeartbeatRegistrationHelper::AttemptRegistration,
                         weak_factory_.GetWeakPtr()),
          kRegistrationRetryDelay);
      break;

    case gcm::GCMClient::INVALID_PARAMETER:
    case gcm::GCMClient::UNKNOWN_ERROR:
    case gcm::GCMClient::GCM_DISABLED:
      // No need to bother retrying in the case of one of these fatal errors.
      // This means that heartbeats will remain disabled until the next
      // restart.
      DLOG(ERROR) << "Fatal GCM Registration error: " << result;
      break;

    case gcm::GCMClient::ASYNC_OPERATION_PENDING:
    case gcm::GCMClient::TTL_EXCEEDED:
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected GCMDriver::Register() result: " << result;
      break;
  }
}

HeartbeatScheduler::HeartbeatScheduler(
    gcm::GCMDriver* driver,
    CloudPolicyClient* cloud_policy_client,
    CloudPolicyStore* cloud_policy_store,
    const std::string& device_id,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner),
      device_id_(device_id),
      heartbeat_enabled_(false),
      heartbeat_interval_(kDefaultHeartbeatInterval),
      cloud_policy_client_(cloud_policy_client),
      cloud_policy_store_(cloud_policy_store),
      gcm_driver_(driver) {
  // If no GCMDriver (e.g. this is loaded as part of an unrelated unit test)
  // do nothing as no heartbeats can be sent.
  if (!gcm_driver_)
    return;

  heartbeat_frequency_subscription_ =
      ash::CrosSettings::Get()->AddSettingsObserver(
          ash::kHeartbeatFrequency,
          base::BindRepeating(&HeartbeatScheduler::RefreshHeartbeatSettings,
                              base::Unretained(this)));

  heartbeat_enabled_subscription_ =
      ash::CrosSettings::Get()->AddSettingsObserver(
          ash::kHeartbeatEnabled,
          base::BindRepeating(&HeartbeatScheduler::RefreshHeartbeatSettings,
                              base::Unretained(this)));

  // Update the heartbeat frequency from settings. This will trigger a
  // heartbeat as appropriate once the settings have been refreshed.
  RefreshHeartbeatSettings();

  // Initialize the default heartbeats interval for GCM driver.
  gcm_driver_->AddHeartbeatInterval(kHeartbeatSchedulerScope,
                                    heartbeat_interval_.InMilliseconds());
}

void HeartbeatScheduler::RefreshHeartbeatSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  ash::CrosSettings* settings = ash::CrosSettings::Get();
  if (ash::CrosSettingsProvider::TRUSTED !=
      settings->PrepareTrustedValues(
          base::BindOnce(&HeartbeatScheduler::RefreshHeartbeatSettings,
                         weak_factory_.GetWeakPtr()))) {
    return;
  }

  // CrosSettings are trusted - update our cached settings (we cache the
  // value because CrosSettings can become untrusted at arbitrary times and we
  // want to use the last trusted value).
  int frequency;
  if (settings->GetInteger(ash::kHeartbeatFrequency, &frequency)) {
    heartbeat_interval_ =
        EnsureValidHeartbeatInterval(base::Milliseconds(frequency));
  }

  gcm_driver_->AddHeartbeatInterval(kHeartbeatSchedulerScope,
                                    heartbeat_interval_.InMilliseconds());

  bool enabled;
  if (settings->GetBoolean(ash::kHeartbeatEnabled, &enabled))
    heartbeat_enabled_ = enabled;

  if (!heartbeat_enabled_) {
    // Heartbeats are no longer enabled - cancel our callback and any
    // outstanding registration attempts and disconnect from GCM so the
    // connection can be shut down. If heartbeats are re-enabled later, we
    // will re-register with GCM.
    ShutdownGCM();
  } else {
    // Schedule a new upload with the new frequency.
    ScheduleNextHeartbeat();
  }

  DVLOG(1) << "heartbeat enabled: " << heartbeat_enabled_;
  DVLOG(1) << "heartbeat frequency: " << heartbeat_interval_;
}

void HeartbeatScheduler::ShutdownGCM() {
  heartbeat_callback_.Cancel();
  registration_helper_.reset();
  registration_id_.clear();
  if (registered_app_handler_) {
    registered_app_handler_ = false;
    gcm_driver_->RemoveHeartbeatInterval(kHeartbeatSchedulerScope);
    gcm_driver_->RemoveAppHandler(kHeartbeatGCMAppID);
    gcm_driver_->RemoveConnectionObserver(this);
  }
}

base::TimeDelta HeartbeatScheduler::EnsureValidHeartbeatInterval(
    const base::TimeDelta& interval) {
  if (interval < kMinHeartbeatInterval) {
    DLOG(WARNING) << "Invalid heartbeat interval: " << interval;
    return kMinHeartbeatInterval;
  }
  if (interval > kMaxHeartbeatInterval) {
    DLOG(WARNING) << "Invalid heartbeat interval: " << interval;
    return kMaxHeartbeatInterval;
  }
  return interval;
}

void HeartbeatScheduler::ScheduleNextHeartbeat() {
  // Do nothing if heartbeats are disabled.
  if (!heartbeat_enabled_)
    return;

  if (registration_id_.empty()) {
    // We are not registered with the GCM service yet, so kick off registration.
    if (!registration_helper_) {
      // Add ourselves as an AppHandler - this is required in order to setup
      // a GCM connection.
      registered_app_handler_ = true;
      gcm_driver_->AddAppHandler(kHeartbeatGCMAppID, this);
      gcm_driver_->AddConnectionObserver(this);
      registration_helper_ = std::make_unique<HeartbeatRegistrationHelper>(
          gcm_driver_, task_runner_);
      registration_helper_->Register(
          base::BindOnce(&HeartbeatScheduler::OnRegistrationComplete,
                         weak_factory_.GetWeakPtr()));
    }
    return;
  }

  // Set the customerId if the policy is fetched.
  if (cloud_policy_store_->policy()) {
    customer_id_ = cloud_policy_store_->policy()->obfuscated_customer_id();
  }

  // Calculate when to fire off the next update (if it should have already
  // happened, this yields a TimeDelta of 0).
  base::TimeDelta delay = std::max(
      last_heartbeat_ + heartbeat_interval_ - base::Time::NowFromSystemTime(),
      base::TimeDelta());

  heartbeat_callback_.Reset(base::BindOnce(&HeartbeatScheduler::SendHeartbeat,
                                           base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, heartbeat_callback_.callback(),
                                delay);
}

void HeartbeatScheduler::OnRegistrationComplete(
    const std::string& registration_id) {
  DCHECK(!registration_id.empty());
  registration_helper_.reset();
  registration_id_ = registration_id;

  if (cloud_policy_client_) {
    // TODO(binjin): Avoid sending the same GCM id to the server.
    // See http://crbug.com/516375
    cloud_policy_client_->UpdateGcmId(
        registration_id,
        base::BindOnce(&HeartbeatScheduler::OnGcmIdUpdateRequestSent,
                       weak_factory_.GetWeakPtr()));
    SignUpUpstreamNotification();
  }

  // Now that GCM registration is complete, start sending heartbeats.
  ScheduleNextHeartbeat();
}

void HeartbeatScheduler::SendHeartbeat() {
  DCHECK(!registration_id_.empty());
  if (!gcm_driver_ || !heartbeat_enabled_ || customer_id_.empty())
    return;

  gcm::OutgoingMessage message;
  message.time_to_live = heartbeat_interval_.InSeconds();
  // Just use the current timestamp as the message ID - if the user changes the
  // time and we send a message with the same ID that we previously used, no
  // big deal (the new message will replace the old, which is the behavior we
  // want anyway, per:
  // https://developer.chrome.com/apps/cloudMessaging#send_messages
  message.id =
      base::NumberToString(base::Time::NowFromSystemTime().ToInternalValue());
  message.data[kGcmMessageTypeKey] = kHeartbeatTypeValue;
  message.data[kHeartbeatTimestampKey] = base::NumberToString(
      base::Time::NowFromSystemTime().InMillisecondsSinceUnixEpoch());
  message.data[kHeartbeatCustomerIdKey] = customer_id_;
  message.data[kHeartbeatDeviceIDKey] = device_id_;
  gcm_driver_->Send(kHeartbeatGCMAppID,
                    GetDestinationID() + kHeartbeatGCMSenderSuffix, message,
                    base::BindOnce(&HeartbeatScheduler::OnHeartbeatSent,
                                   weak_factory_.GetWeakPtr()));
}

void HeartbeatScheduler::SignUpUpstreamNotification() {
  DCHECK(gcm_driver_);

  // Registration ID is a hard requirement for upstream notification signup,
  // so we need to send the sign up message after registration is completed,
  // as well as registration ID changes.
  // We also listen to GCM driver connected events, so that once we
  // reconnected to GCM server, we can resend the signup message immediately.
  // Having both conditional checks here ensures that during the start up, the
  // sign up message will be sent at most once.
  if (registration_id_.empty() || !gcm_driver_->IsConnected())
    return;

  gcm::OutgoingMessage message;
  message.id =
      base::NumberToString(base::Time::NowFromSystemTime().ToInternalValue());
  message.data[kGcmMessageTypeKey] = kUpstreamNotificationSignUpListeningEvents;
  message.data[kUpstreamNotificationNotifyKey] =
      GetDestinationID() + kHeartbeatGCMSenderSuffix;
  message.data[kUpstreamNotificationRegIdKey] = registration_id_;
  gcm_driver_->Send(
      kHeartbeatGCMAppID, kUpstreamNotificationSignUpDestinationID, message,
      base::BindOnce(&HeartbeatScheduler::OnUpstreamNotificationSent,
                     weak_factory_.GetWeakPtr()));
}

void HeartbeatScheduler::OnHeartbeatSent(const std::string& message_id,
                                         gcm::GCMClient::Result result) {
  DVLOG(1) << "Monitoring heartbeat sent - result = " << result;
  // Don't care if the result was successful or not - just schedule the next
  // heartbeat.
  DLOG_IF(ERROR, result != gcm::GCMClient::SUCCESS)
      << "Error sending monitoring heartbeat: " << result;

  last_heartbeat_ = base::Time::NowFromSystemTime();
  ScheduleNextHeartbeat();
}

void HeartbeatScheduler::OnUpstreamNotificationSent(
    const std::string& message_id,
    gcm::GCMClient::Result result) {
  DVLOG(1) << "Upstream notification signup message sent - result = " << result;
  DLOG_IF(ERROR, result != gcm::GCMClient::SUCCESS)
      << "Error sending upstream notification signup message: " << result;
}

HeartbeatScheduler::~HeartbeatScheduler() {
  ShutdownGCM();
}

void HeartbeatScheduler::ShutdownHandler() {
  // This should never be called, because BrowserProcessImpl::StartTearDown()
  // should shutdown the BrowserPolicyConnector (which destroys this object)
  // before the GCMDriver. Our goal is to make sure that this object is always
  // shutdown before GCMDriver is shut down, rather than trying to handle the
  // case when GCMDriver goes away.
  NOTREACHED_IN_MIGRATION()
      << "HeartbeatScheduler should be destroyed before GCMDriver";
}

void HeartbeatScheduler::OnStoreReset() {
  // TODO(crbug.com/40491756): Tell server that |registration_id_| is no longer
  // valid. See also crbug.com/516375.
  if (!registration_helper_) {
    ShutdownGCM();
    RefreshHeartbeatSettings();
  }  // Otherwise let the pending registration complete normally.
}

void HeartbeatScheduler::OnMessage(const std::string& app_id,
                                   const gcm::IncomingMessage& message) {
  // Should never be called because we don't get any incoming messages
  // for our app ID.
  NOTREACHED_IN_MIGRATION() << "Received incoming message for " << app_id;
}

void HeartbeatScheduler::OnMessagesDeleted(const std::string& app_id) {}

void HeartbeatScheduler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  // Ignore send errors - we already are notified above in OnHeartbeatSent().
}

void HeartbeatScheduler::OnSendAcknowledged(const std::string& app_id,
                                            const std::string& message_id) {
  DVLOG(1) << "Heartbeat sent with message_id: " << message_id;
}

void HeartbeatScheduler::OnConnected(const net::IPEndPoint&) {
  SignUpUpstreamNotification();
}

void HeartbeatScheduler::OnGcmIdUpdateRequestSent(bool success) {
  // TODO(binjin): Handle the failure, probably by exponential backoff.
  LOG_IF(WARNING, !success) << "Failed to send GCM id to DM server";
}

}  // namespace policy
