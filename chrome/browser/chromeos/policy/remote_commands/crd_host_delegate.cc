// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/crd_host_delegate.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "remoting/host/it2me/it2me_native_messaging_host_chromeos.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

// TODO(https://crbug.com/864455): move these constants to some place
// that they can be reused by both this code and It2MeNativeMessagingHost.

// Communication with CRD Host, messages sent to host:
constexpr char kCRDMessageTypeKey[] = "type";

constexpr char kCRDMessageHello[] = "hello";
constexpr char kCRDMessageConnect[] = "connect";
constexpr char kCRDMessageDisconnect[] = "disconnect";

// Communication with CRD Host, messages received from host:
constexpr char kCRDResponseHello[] = "helloResponse";
constexpr char kCRDResponseConnect[] = "connectResponse";
constexpr char kCRDStateChanged[] = "hostStateChanged";
constexpr char kCRDResponseDisconnect[] = "disconnectResponse";
constexpr char kCRDResponseError[] = "error";

// Connect message parameters:
constexpr char kCRDConnectUserName[] = "userName";
constexpr char kCRDConnectAuth[] = "authServiceWithToken";
constexpr char kCRDConnectXMPPServer[] = "xmppServerAddress";
constexpr char kCRDConnectXMPPTLS[] = "xmppServerUseTls";
constexpr char kCRDConnectDirectoryBot[] = "directoryBotJid";
constexpr char kCRDConnectSuppressUserDialogs[] = "suppressUserDialogs";
constexpr char kCRDConnectSuppressNotifications[] = "suppressNotifications";
constexpr char kCRDTerminateUponInput[] = "terminateUponInput";

// Connect message parameter values:
constexpr char kCRDConnectXMPPServerValue[] = "talk.google.com:443";
constexpr char kCRDConnectDirectoryBotValue[] = "remoting@bot.talk.google.com";

// CRD host states we care about:
constexpr char kCRDStateKey[] = "state";
constexpr char kCRDStateError[] = "ERROR";
constexpr char kCRDStateStarting[] = "STARTING";
constexpr char kCRDStateAccessCodeRequested[] = "REQUESTED_ACCESS_CODE";
constexpr char kCRDStateDomainError[] = "INVALID_DOMAIN_ERROR";
constexpr char kCRDStateAccessCode[] = "RECEIVED_ACCESS_CODE";
constexpr char kCRDStateRemoteDisconnected[] = "DISCONNECTED";
constexpr char kCRDStateRemoteConnected[] = "CONNECTED";

constexpr char kCRDErrorCodeKey[] = "error_code";
constexpr char kCRDAccessCodeKey[] = "accessCode";
constexpr char kCRDAccessCodeLifetimeKey[] = "accessCodeLifetime";

constexpr char kCRDConnectClientKey[] = "client";

// OAuth2 Token scopes
constexpr char kCloudDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/clouddevices";
constexpr char kChromotingRemoteSupportOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromoting.remote.support";
constexpr char kTachyonOAuth2Scope[] =
    "https://www.googleapis.com/auth/tachyon";

}  // namespace

// Helper class that asynchronously fetches the OAuth token, and passes it to
// the given callback.
class CRDHostDelegate::OAuthTokenFetcher
    : public OAuth2AccessTokenManager::Consumer {
 public:
  OAuthTokenFetcher(
      DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback)
      : OAuth2AccessTokenManager::Consumer("crd_host_delegate"),
        success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)) {}
  OAuthTokenFetcher(const OAuthTokenFetcher&) = delete;
  OAuthTokenFetcher& operator=(const OAuthTokenFetcher&) = delete;
  ~OAuthTokenFetcher() override = default;

  void Start() {
    DeviceOAuth2TokenService* oauth_service =
        DeviceOAuth2TokenServiceFactory::Get();

    OAuth2AccessTokenManager::ScopeSet scopes{
        GaiaConstants::kGoogleUserInfoEmail, kCloudDevicesOAuth2Scope,
        kChromotingRemoteSupportOAuth2Scope, kTachyonOAuth2Scope};

    oauth_request_ = oauth_service->StartAccessTokenRequest(scopes, this);
  }

  bool is_running() const { return oauth_request_ != nullptr; }

 private:
  // OAuth2AccessTokenManager::Consumer implementation:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    std::move(success_callback_).Run(token_response.access_token);
    oauth_request_.reset();
  }

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    std::move(error_callback_)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_OAUTH_TOKEN,
             error.ToString());
    oauth_request_.reset();
  }

  DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback_;
  DeviceCommandStartCRDSessionJob::ErrorCallback error_callback_;
  // Handler for the OAuth access token request.
  // When deleted the token manager will cancel the request (and not call us).
  std::unique_ptr<OAuth2AccessTokenManager::Request> oauth_request_;
};

CRDHostDelegate::CRDHostDelegate() = default;

CRDHostDelegate::~CRDHostDelegate() {}

bool CRDHostDelegate::HasActiveSession() const {
  return host_ != nullptr;
}

void CRDHostDelegate::TerminateSession(base::OnceClosure callback) {
  DoShutdownHost();
  std::move(callback).Run();
}

bool CRDHostDelegate::AreServicesReady() const {
  return user_manager::UserManager::IsInitialized() &&
         ui::UserActivityDetector::Get() != nullptr &&
         chromeos::ProfileHelper::Get() != nullptr &&
         DeviceOAuth2TokenServiceFactory::Get() != nullptr;
}

bool CRDHostDelegate::IsRunningKiosk() const {
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsLoggedInAsAnyKioskApp()) {
    return false;
  }
  if (!GetKioskProfile())
    return false;

  if (user_manager->IsLoggedInAsKioskApp()) {
    ash::KioskAppManager* manager = ash::KioskAppManager::Get();
    if (manager->GetAutoLaunchApp().empty())
      return false;
    ash::KioskAppManager::App app;
    CHECK(manager->GetApp(manager->GetAutoLaunchApp(), &app));
    return app.was_auto_launched_with_zero_delay;
  } else if (user_manager->IsLoggedInAsArcKioskApp()) {
    return chromeos::ArcKioskAppManager::Get()
        ->current_app_was_auto_launched_with_zero_delay();
  } else if (user_manager->IsLoggedInAsWebKioskApp()) {
    return ash::WebKioskAppManager::Get()
        ->current_app_was_auto_launched_with_zero_delay();
  }
  NOTREACHED();
  return false;
}

base::TimeDelta CRDHostDelegate::GetIdlenessPeriod() const {
  return base::TimeTicks::Now() -
         ui::UserActivityDetector::Get()->last_activity_time();
}

void CRDHostDelegate::FetchOAuthToken(
    DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  DCHECK(!oauth_token_fetcher_ || !oauth_token_fetcher_->is_running());

  oauth_token_fetcher_ = std::make_unique<OAuthTokenFetcher>(
      std::move(success_callback), std::move(error_callback));
  oauth_token_fetcher_->Start();
}

void CRDHostDelegate::StartCRDHostAndGetCode(
    const std::string& oauth_token,
    bool terminate_upon_input,
    DeviceCommandStartCRDSessionJob::AccessCodeCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  DCHECK(!host_);
  DCHECK(!code_success_callback_);
  DCHECK(!error_callback_);

  // Store all parameters for future connect call.
  base::Value connect_params(base::Value::Type::DICTIONARY);
  CoreAccountId account_id =
      DeviceOAuth2TokenServiceFactory::Get()->GetRobotAccountId();

  // TODO(msarda): This conversion will not be correct once account id is
  // migrated to be the Gaia ID on ChromeOS. Fix it.
  std::string username = account_id.ToString();

  connect_params.SetKey(kCRDConnectUserName, base::Value(username));
  connect_params.SetKey(kCRDConnectAuth, base::Value("oauth2:" + oauth_token));
  connect_params.SetKey(kCRDConnectXMPPServer,
                        base::Value(kCRDConnectXMPPServerValue));
  connect_params.SetKey(kCRDConnectXMPPTLS, base::Value(true));
  connect_params.SetKey(kCRDConnectDirectoryBot,
                        base::Value(kCRDConnectDirectoryBotValue));
  connect_params.SetKey(kCRDConnectSuppressUserDialogs, base::Value(true));
  connect_params.SetKey(kCRDConnectSuppressNotifications, base::Value(true));
  connect_params.SetKey(kCRDTerminateUponInput,
                        base::Value(terminate_upon_input));
  connect_params_ = std::move(connect_params);

  remote_connected_ = false;
  command_awaiting_crd_access_code_ = true;

  code_success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  // TODO(antrim): set up watchdog timer (reasonable cutoff).
  host_ = remoting::CreateIt2MeNativeMessagingHostForChromeOS(
      content::GetIOThreadTaskRunner({}), content::GetUIThreadTaskRunner({}),
      g_browser_process->policy_service());
  host_->Start(this);

  base::Value params(base::Value::Type::DICTIONARY);
  SendMessageToHost(kCRDMessageHello, params);
}

void CRDHostDelegate::PostMessageFromNativeHost(const std::string& message) {
  std::unique_ptr<base::Value> message_value =
      base::JSONReader::ReadDeprecated(message);
  if (!message_value->is_dict()) {
    OnProtocolBroken("Message is not a dictionary");
    return;
  }

  auto* type_value = message_value->FindKeyOfType(kCRDMessageTypeKey,
                                                  base::Value::Type::STRING);
  if (!type_value) {
    OnProtocolBroken("Message without type");
    return;
  }
  std::string type = type_value->GetString();

  if (type == kCRDResponseHello) {
    OnHelloResponse();
    return;
  } else if (type == kCRDResponseConnect) {
    // Ok, just ignore.
    return;
  } else if (type == kCRDResponseDisconnect) {
    OnDisconnectResponse();
    return;
  } else if (type == kCRDStateChanged || type == kCRDResponseError) {
    // Handle CRD host state changes
    auto* state_value =
        message_value->FindKeyOfType(kCRDStateKey, base::Value::Type::STRING);
    if (!state_value) {
      OnProtocolBroken("No state in message");
      return;
    }
    std::string state = state_value->GetString();

    if (state == kCRDStateAccessCode) {
      OnStateReceivedAccessCode(*message_value);
    } else if (state == kCRDStateRemoteConnected) {
      OnStateRemoteConnected(*message_value);
    } else if (state == kCRDStateRemoteDisconnected) {
      OnStateRemoteDisconnected();
    } else if (state == kCRDStateError || state == kCRDStateDomainError) {
      OnStateError(state, *message_value);
    } else if (state == kCRDStateStarting ||
               state == kCRDStateAccessCodeRequested) {
      // Just ignore these states.
    } else {
      LOG(WARNING) << "Unhandled state :" << type;
    }
    return;
  }
  LOG(WARNING) << "Unknown message type: " << type;
}

void CRDHostDelegate::OnHelloResponse() {
  // Host is initialized, start connection.
  SendMessageToHost(kCRDMessageConnect, connect_params_);
}

void CRDHostDelegate::OnDisconnectResponse() {
  // Should happen only when remoting session finished and we
  // have requested host to shut down, or when we have got second auth code
  // without receiving connection.
  DCHECK(!command_awaiting_crd_access_code_);
  DCHECK(!remote_connected_);
  ShutdownHost();
}

void CRDHostDelegate::OnStateError(std::string error_state,
                                   base::Value& message) {
  std::string error_message;
  if (error_state == kCRDStateDomainError) {
    error_message = "CRD Error : Invalid domain";
  } else {
    auto* error_code_value =
        message.FindKeyOfType(kCRDErrorCodeKey, base::Value::Type::STRING);
    if (error_code_value)
      error_message = error_code_value->GetString();
    else
      error_message = "Unknown CRD Error";
  }
  // Notify callback if command is still running.
  if (command_awaiting_crd_access_code_) {
    command_awaiting_crd_access_code_ = false;
    std::move(error_callback_)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_CRD_HOST_ERROR,
             "CRD Error state " + error_state);
    code_success_callback_.Reset();
  }
  // Shut down host, if any
  ShutdownHost();
}

void CRDHostDelegate::OnStateRemoteConnected(base::Value& message) {
  remote_connected_ = true;
  // TODO(antrim): set up watchdog timer (session duration).
  auto* client_value =
      message.FindKeyOfType(kCRDConnectClientKey, base::Value::Type::STRING);
  if (client_value) {
    VLOG(1) << "Remote connection by " << client_value->GetString();
  }
}

void CRDHostDelegate::OnStateRemoteDisconnected() {
  // There could be a connection attempt that was not successful, we will
  // receive "disconnected" message without actually receiving "connected".
  if (!remote_connected_)
    return;
  remote_connected_ = false;
  // Remote has disconnected, time to send "disconnect" that would result
  // in shutting down the host.
  base::Value params(base::Value::Type::DICTIONARY);
  SendMessageToHost(kCRDMessageDisconnect, params);
}

void CRDHostDelegate::OnStateReceivedAccessCode(base::Value& message) {
  if (!command_awaiting_crd_access_code_) {
    if (!remote_connected_) {
      // We have already sent the access code back to the server which initiated
      // this CRD session through a remote command, and we can not send a new
      // access code. Assuming that the old access code is no longer valid, we
      // can only terminate the current CRD session.
      base::Value params(base::Value::Type::DICTIONARY);
      SendMessageToHost(kCRDMessageDisconnect, params);
    }
    return;
  }

  auto* code_value =
      message.FindKeyOfType(kCRDAccessCodeKey, base::Value::Type::STRING);
  auto* code_lifetime_value = message.FindKeyOfType(kCRDAccessCodeLifetimeKey,
                                                    base::Value::Type::INTEGER);
  if (!code_value || !code_lifetime_value) {
    OnProtocolBroken("Can not obtain access code");
    return;
  }
  // TODO(antrim): set up watchdog timer (access code lifetime).
  command_awaiting_crd_access_code_ = false;
  std::move(code_success_callback_).Run(std::string(code_value->GetString()));
  error_callback_.Reset();
}

void CRDHostDelegate::CloseChannel(const std::string& error_message) {
  LOG(ERROR) << "CRD Host closed channel" << error_message;
  command_awaiting_crd_access_code_ = false;

  if (error_callback_) {
    std::move(error_callback_)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_CRD_HOST_ERROR,
             error_message);
  }
  code_success_callback_.Reset();
  ShutdownHost();
}

void CRDHostDelegate::SendMessageToHost(const std::string& type,
                                        base::Value& params) {
  std::string message_json;
  params.SetKey(kCRDMessageTypeKey, base::Value(type));
  base::JSONWriter::Write(params, &message_json);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CRDHostDelegate::DoSendMessage,
                                weak_factory_.GetWeakPtr(), message_json));
}

void CRDHostDelegate::DoSendMessage(const std::string& json) {
  if (!host_)
    return;
  host_->OnMessage(json);
}

void CRDHostDelegate::OnProtocolBroken(const std::string& message) {
  LOG(ERROR) << "Error communicating with CRD Host : " << message;
  command_awaiting_crd_access_code_ = false;

  std::move(error_callback_)
      .Run(DeviceCommandStartCRDSessionJob::FAILURE_CRD_HOST_ERROR, message);
  code_success_callback_.Reset();
  ShutdownHost();
}

void CRDHostDelegate::ShutdownHost() {
  if (!host_)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CRDHostDelegate::DoShutdownHost,
                                weak_factory_.GetWeakPtr()));
}

void CRDHostDelegate::DoShutdownHost() {
  host_.reset();
}

Profile* CRDHostDelegate::GetKioskProfile() const {
  auto* user_manager = user_manager::UserManager::Get();
  return chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager->GetActiveUser());
}

}  // namespace policy
