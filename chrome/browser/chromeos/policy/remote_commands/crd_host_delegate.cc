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
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/it2me/it2me_native_messaging_host_chromeos.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

// Add a common prefix to all our logs, to make them easy to find.
#define CRD_DVLOG(level) DVLOG(level) << "CRD: "
#define CRD_LOG(level) LOG(level) << "CRD: "

// OAuth2 Token scopes
constexpr char kCloudDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/clouddevices";
constexpr char kChromotingRemoteSupportOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromoting.remote.support";
constexpr char kTachyonOAuth2Scope[] =
    "https://www.googleapis.com/auth/tachyon";

class DefaultNativeMessageHostFactory
    : public CRDHostDelegate::NativeMessageHostFactory {
 public:
  DefaultNativeMessageHostFactory() = default;
  DefaultNativeMessageHostFactory(const DefaultNativeMessageHostFactory&) =
      delete;
  DefaultNativeMessageHostFactory& operator=(
      const DefaultNativeMessageHostFactory&) = delete;
  ~DefaultNativeMessageHostFactory() override = default;

  // CRDHostDelegate::NativeMessageHostFactory implementation:
  std::unique_ptr<extensions::NativeMessageHost> CreateNativeMessageHostHost()
      override {
    return remoting::CreateIt2MeNativeMessagingHostForChromeOS(
        content::GetIOThreadTaskRunner({}), content::GetUIThreadTaskRunner({}),
        g_browser_process->policy_service());
  }
};

std::string FormatErrorMessage(const std::string& error_state,
                               const base::Value& message) {
  if (error_state == remoting::kHostStateDomainError) {
    return "Invalid domain";
  } else {
    const std::string* error_code =
        message.FindStringKey(remoting::kErrorMessageCode);
    if (error_code)
      return *error_code;
    else
      return "Unknown Error";
  }
}

}  // namespace

// Helper class that asynchronously fetches the OAuth token, and passes it to
// the given callback.
class CRDHostDelegate::OAuthTokenFetcher
    : public OAuth2AccessTokenManager::Consumer {
 public:
  OAuthTokenFetcher(
      DeviceOAuth2TokenService* oauth_service,
      DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback)
      : OAuth2AccessTokenManager::Consumer("crd_host_delegate"),
        oauth_service_(*oauth_service),
        success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)) {
    DCHECK(oauth_service);
  }
  OAuthTokenFetcher(const OAuthTokenFetcher&) = delete;
  OAuthTokenFetcher& operator=(const OAuthTokenFetcher&) = delete;
  ~OAuthTokenFetcher() override = default;

  void Start() {
    CRD_DVLOG(1) << "Fetching OAuth access token";
    OAuth2AccessTokenManager::ScopeSet scopes{
        GaiaConstants::kGoogleUserInfoEmail, kCloudDevicesOAuth2Scope,
        kChromotingRemoteSupportOAuth2Scope, kTachyonOAuth2Scope};
    oauth_request_ = oauth_service_.StartAccessTokenRequest(scopes, this);
  }

  bool is_running() const { return oauth_request_ != nullptr; }

 private:
  // OAuth2AccessTokenManager::Consumer implementation:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    CRD_DVLOG(1) << "Received OAuth access token";
    std::move(success_callback_).Run(token_response.access_token);
    oauth_request_.reset();
  }

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    CRD_DVLOG(1) << "Failed to get OAuth access token: " << error.ToString();
    std::move(error_callback_)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_OAUTH_TOKEN,
             error.ToString());
    oauth_request_.reset();
  }

  DeviceOAuth2TokenService& oauth_service_;
  DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback_;
  DeviceCommandStartCRDSessionJob::ErrorCallback error_callback_;
  // Handler for the OAuth access token request.
  // When deleted the token manager will cancel the request (and not call us).
  std::unique_ptr<OAuth2AccessTokenManager::Request> oauth_request_;
};

CRDHostDelegate::CRDHostDelegate()
    : CRDHostDelegate(std::make_unique<DefaultNativeMessageHostFactory>()) {}

CRDHostDelegate::CRDHostDelegate(
    std::unique_ptr<NativeMessageHostFactory> factory)
    : factory_(std::move(factory)) {
  DCHECK(factory_);
}

CRDHostDelegate::~CRDHostDelegate() = default;

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
         oauth_service() != nullptr;
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
  DCHECK(oauth_service());
  DCHECK(!oauth_token_fetcher_ || !oauth_token_fetcher_->is_running());

  oauth_token_fetcher_ = std::make_unique<OAuthTokenFetcher>(
      oauth_service(), std::move(success_callback), std::move(error_callback));
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
  DCHECK(oauth_service());

  // Store all parameters for future connect call.
  base::Value connect_params(base::Value::Type::DICTIONARY);
  CoreAccountId account_id = oauth_service()->GetRobotAccountId();

  // TODO(msarda): This conversion will not be correct once account id is
  // migrated to be the Gaia ID on ChromeOS. Fix it.
  std::string username = account_id.ToString();

  connect_params.SetKey(remoting::kUserName, base::Value(username));
  connect_params.SetKey(remoting::kAuthServiceWithToken,
                        base::Value("oauth2:" + oauth_token));
  connect_params.SetKey(remoting::kSuppressUserDialogs, base::Value(true));
  connect_params.SetKey(remoting::kSuppressNotifications, base::Value(true));
  connect_params.SetKey(remoting::kTerminateUponInput,
                        base::Value(terminate_upon_input));
  connect_params_ = std::move(connect_params);

  remote_connected_ = false;
  command_awaiting_crd_access_code_ = true;

  code_success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  // TODO(antrim): set up watchdog timer (reasonable cutoff).
  host_ = factory_->CreateNativeMessageHostHost();
  host_->Start(this);

  base::Value params(base::Value::Type::DICTIONARY);
  SendMessageToHost(remoting::kHelloMessage, params);
}

void CRDHostDelegate::PostMessageFromNativeHost(
    const std::string& message_string) {
  CRD_DVLOG(1) << "Received message from CRD host: " << message_string;

  absl::optional<base::Value> message = base::JSONReader::Read(message_string);
  if (!message) {
    OnProtocolBroken("Message is invalid JSON");
    return;
  }

  if (!message->is_dict()) {
    OnProtocolBroken("Message is not a dictionary");
    return;
  }

  const std::string* type_pointer =
      message->FindStringKey(remoting::kMessageType);
  if (!type_pointer) {
    OnProtocolBroken("Message without type");
    return;
  }
  const std::string& type = *type_pointer;

  if (type == remoting::kHelloResponse) {
    OnHelloResponse();
    return;
  } else if (type == remoting::kConnectResponse) {
    //  Ok, just ignore.
    return;
  } else if (type == remoting::kDisconnectResponse) {
    OnDisconnectResponse();
    return;
  } else if (type == remoting::kHostStateChangedMessage ||
             type == remoting::kErrorMessage) {
    //  Handle CRD host state changes
    const std::string* state_pointer = message->FindStringKey(remoting::kState);
    if (!state_pointer) {
      OnProtocolBroken("No state in message");
      return;
    }
    const std::string& state = *state_pointer;

    if (state == remoting::kHostStateReceivedAccessCode) {
      OnStateReceivedAccessCode(*message);
    } else if (state == remoting::kHostStateConnected) {
      OnStateRemoteConnected(*message);
    } else if (state == remoting::kHostStateDisconnected) {
      OnStateRemoteDisconnected();
    } else if (state == remoting::kHostStateError ||
               state == remoting::kHostStateDomainError) {
      OnStateError(state, *message);
    } else if (state == remoting::kHostStateStarting ||
               state == remoting::kHostStateRequestedAccessCode) {
      //  Just ignore these states.
    } else {
      CRD_LOG(WARNING) << "Unhandled state :" << type;
    }
    return;
  }
  CRD_LOG(WARNING) << "Unknown message type: " << type;
}

void CRDHostDelegate::OnHelloResponse() {
  // Host is initialized, start connection.
  SendMessageToHost(remoting::kConnectMessage, connect_params_);
}

void CRDHostDelegate::OnDisconnectResponse() {
  // Should happen only when remoting session finished and we
  // have requested host to shut down, or when we have got second auth code
  // without receiving connection.
  DCHECK(!command_awaiting_crd_access_code_);
  DCHECK(!remote_connected_);
  ShutdownHost();
}

void CRDHostDelegate::OnStateError(const std::string& error_state,
                                   const base::Value& message) {
  // Notify callback if command is still running.
  if (command_awaiting_crd_access_code_) {
    command_awaiting_crd_access_code_ = false;
    std::move(error_callback_)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_CRD_HOST_ERROR,
             "CRD State Error: " + FormatErrorMessage(error_state, message));
    code_success_callback_.Reset();
  }
  // Shut down host, if any.
  ShutdownHost();
}

void CRDHostDelegate::OnStateRemoteConnected(const base::Value& message) {
  remote_connected_ = true;
  // TODO(antrim): set up watchdog timer (session duration).
  const std::string* client = message.FindStringKey(remoting::kClient);
  if (client)
    CRD_DVLOG(1) << "Remote connection by " << *client;
}

void CRDHostDelegate::OnStateRemoteDisconnected() {
  // There could be a connection attempt that was not successful, we will
  // receive "disconnected" message without actually receiving "connected".
  if (!remote_connected_) {
    CRD_DVLOG(1) << "Received disconnect out-of-order before connect";
    return;
  }
  remote_connected_ = false;
  // Remote has disconnected, time to send "disconnect" that would result
  // in shutting down the host.
  base::Value params(base::Value::Type::DICTIONARY);
  SendMessageToHost(remoting::kDisconnectMessage, params);
}

void CRDHostDelegate::OnStateReceivedAccessCode(const base::Value& message) {
  if (!command_awaiting_crd_access_code_) {
    if (!remote_connected_) {
      // We have already sent the access code back to the server which initiated
      // this CRD session through a remote command, and we can not send a new
      // access code. Assuming that the old access code is no longer valid, we
      // can only terminate the current CRD session.
      base::Value params(base::Value::Type::DICTIONARY);
      SendMessageToHost(remoting::kDisconnectMessage, params);
    }
    return;
  }

  const std::string* access_code = message.FindStringKey(remoting::kAccessCode);
  absl::optional<int> code_lifetime =
      message.FindIntKey(remoting::kAccessCodeLifetime);
  if (!access_code || !code_lifetime) {
    OnProtocolBroken("Can not obtain access code");
    return;
  }

  CRD_DVLOG(1) << "Got access code";
  // TODO(antrim): set up watchdog timer (access code lifetime).
  command_awaiting_crd_access_code_ = false;
  std::move(code_success_callback_).Run(*access_code);
  error_callback_.Reset();
}

void CRDHostDelegate::CloseChannel(const std::string& error_message) {
  CRD_LOG(ERROR) << "CRD Host closed channel" << error_message;
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
  CRD_DVLOG(1) << "Sending message of type '" << type << "' to CRD host.";
  std::string message_json;
  params.SetKey(remoting::kMessageType, base::Value(type));
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
  CRD_LOG(ERROR) << "Error communicating with CRD Host : " << message;
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

DeviceOAuth2TokenService* CRDHostDelegate::oauth_service() const {
  return DeviceOAuth2TokenServiceFactory::Get();
}

}  // namespace policy
