// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/crd_host_delegate.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "remoting/host/it2me/it2me_native_messaging_host_chromeos.h"
#include "services/network/public/cpp/simple_url_loader.h"
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

// Connect message parameters:
constexpr char kCRDConnectUserName[] = "userName";
constexpr char kCRDConnectAuth[] = "authServiceWithToken";
constexpr char kCRDConnectXMPPServer[] = "xmppServerAddress";
constexpr char kCRDConnectXMPPTLS[] = "xmppServerUseTls";
constexpr char kCRDConnectDirectoryBot[] = "directoryBotJid";
constexpr char kCRDConnectICEConfig[] = "iceConfig";
constexpr char kCRDConnectNoDialogs[] = "noDialogs";

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

constexpr char kICEConfigURL[] =
    "https://www.googleapis.com/chromoting/v1/@me/iceconfig";

// OAuth2 Token scopes
constexpr char kCloudDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/clouddevices";
constexpr char kChromotingOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromoting";

net::NetworkTrafficAnnotationTag CreateIceConfigRequestAnnotation() {
  return net::DefineNetworkTrafficAnnotation("CRD_ice_config_request", R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Request is used by Chrome Remote Desktop to fetch ICE "
            "configuration which contains list of STUN & TURN servers and TURN "
            "credentials."
          trigger:
            "When a Chrome Remote Desktop session is being connected and "
            "periodically while a session is active, as necessary. Currently "
            "the API issues credentials that expire every 24 hours, so this "
            "request will only be sent again while session is active more than "
            "24 hours and it needs to renegotiate the ICE connection. The 24 "
            "hour period is controlled by the server and may change. In some "
            "cases, e.g. if direct connection is used, it will not trigger "
            "periodically."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings. You can block Chrome "
            "Remote Desktop as specified here: "
            "https://support.google.com/chrome/?p=remote_desktop"
          chrome_policy {
            RemoteAccessHostFirewallTraversal {
              policy_options {mode: MANDATORY}
              RemoteAccessHostFirewallTraversal: false
            }
          }
        }
        comments:
          "Above specified policy is only applicable on the host side and "
          "doesn't have effect in Android and iOS client apps. The product "
          "is shipped separately from Chromium, except on Chrome OS."
        )");
}

}  // namespace

CRDHostDelegate::CRDHostDelegate()
    : OAuth2TokenService::Consumer("crd_host_delegate"), weak_factory_(this) {}

CRDHostDelegate::~CRDHostDelegate() {
}

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
         chromeos::DeviceOAuth2TokenServiceFactory::Get() != nullptr;
}

bool CRDHostDelegate::IsRunningKiosk() const {
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsLoggedInAsKioskApp() &&
      !user_manager->IsLoggedInAsArcKioskApp()) {
    return false;
  }
  if (!GetKioskProfile())
    return false;

  if (user_manager->IsLoggedInAsKioskApp()) {
    chromeos::KioskAppManager* manager = chromeos::KioskAppManager::Get();
    if (manager->GetAutoLaunchApp().empty())
      return false;
    chromeos::KioskAppManager::App app;
    CHECK(manager->GetApp(manager->GetAutoLaunchApp(), &app));
    return app.was_auto_launched_with_zero_delay;
  } else {  // ARC Kiosk
    return chromeos::ArcKioskAppManager::Get()
        ->current_app_was_auto_launched_with_zero_delay();
  }
}

base::TimeDelta CRDHostDelegate::GetIdlenessPeriod() const {
  return base::TimeTicks::Now() -
         ui::UserActivityDetector::Get()->last_activity_time();
}

void CRDHostDelegate::FetchOAuthToken(
    DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  DCHECK(!oauth_success_callback_);
  DCHECK(!error_callback_);
  chromeos::DeviceOAuth2TokenService* oauth_service =
      chromeos::DeviceOAuth2TokenServiceFactory::Get();

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
  scopes.insert(GaiaConstants::kGoogleTalkOAuth2Scope);
  scopes.insert(kCloudDevicesOAuth2Scope);
  scopes.insert(kChromotingOAuth2Scope);

  oauth_success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  oauth_request_ = oauth_service->StartRequest(
      oauth_service->GetRobotAccountId(), scopes, this);
}

void CRDHostDelegate::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  oauth_request_.reset();
  error_callback_.Reset();
  std::move(oauth_success_callback_).Run(token_response.access_token);
}

void CRDHostDelegate::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  oauth_request_.reset();
  oauth_success_callback_.Reset();
  std::move(error_callback_)
      .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_OAUTH_TOKEN,
           error.ToString());
}

void CRDHostDelegate::FetchICEConfig(
    const std::string& oauth_token,
    DeviceCommandStartCRDSessionJob::ICEConfigCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  DCHECK(!ice_success_callback_);
  DCHECK(!error_callback_);

  ice_success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  auto ice_request = std::make_unique<network::ResourceRequest>();
  ice_request->url = GURL(kICEConfigURL);
  ice_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;

  ice_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                 "Bearer " + oauth_token);
  auto loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(GetKioskProfile())
          ->GetURLLoaderFactoryForBrowserProcess();

  ice_config_loader_ = network::SimpleURLLoader::Create(
      std::move(ice_request), CreateIceConfigRequestAnnotation());
  ice_config_loader_->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&CRDHostDelegate::OnICEConfigurationLoaded,
                     weak_factory_.GetWeakPtr()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void CRDHostDelegate::OnICEConfigurationLoaded(
    std::unique_ptr<std::string> response_body) {
  ice_config_loader_.reset();
  if (response_body) {
    std::unique_ptr<base::Value> value = base::JSONReader::Read(*response_body);
    if (!value || !value->is_dict()) {
      ice_success_callback_.Reset();
      std::move(error_callback_)
          .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_ICE_CONFIG,
               "Could not parse config");
      return;
    }
    auto* config = value->FindKeyOfType("data", base::Value::Type::DICTIONARY);
    if (config) {
      error_callback_.Reset();
      std::move(ice_success_callback_).Run(std::move(*config));
      return;
    }
  }

  ice_success_callback_.Reset();
  std::move(error_callback_)
      .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_ICE_CONFIG,
           std::string());
}

void CRDHostDelegate::StartCRDHostAndGetCode(
    const std::string& oauth_token,
    base::Value ice_config,
    DeviceCommandStartCRDSessionJob::AccessCodeCallback success_callback,
    DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) {
  DCHECK(!host_);
  DCHECK(!code_success_callback_);
  DCHECK(!error_callback_);

  // Store all parameters for future connect call.
  base::Value connect_params(base::Value::Type::DICTIONARY);
  std::string username =
      chromeos::DeviceOAuth2TokenServiceFactory::Get()->GetRobotAccountId();

  connect_params.SetKey(kCRDConnectUserName, base::Value(username));
  connect_params.SetKey(kCRDConnectAuth, base::Value("oauth2:" + oauth_token));
  connect_params.SetKey(kCRDConnectXMPPServer,
                        base::Value(kCRDConnectXMPPServerValue));
  connect_params.SetKey(kCRDConnectXMPPTLS, base::Value(true));
  connect_params.SetKey(kCRDConnectDirectoryBot,
                        base::Value(kCRDConnectDirectoryBotValue));
  connect_params.SetKey(kCRDConnectICEConfig, std::move(ice_config));
  connect_params.SetKey(kCRDConnectNoDialogs, base::Value(true));
  connect_params_ = std::move(connect_params);

  remote_connected_ = false;
  command_awaiting_crd_access_code_ = true;

  code_success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);

  // TODO(antrim): set up watchdog timer (reasonable cutoff).
  host_ = remoting::CreateIt2MeNativeMessagingHostForChromeOS(
      g_browser_process->system_request_context(),
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO}),
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI}),
      g_browser_process->policy_service());
  host_->Start(this);

  base::Value params(base::Value::Type::DICTIONARY);
  SendMessageToHost(kCRDMessageHello, params);
}

void CRDHostDelegate::PostMessageFromNativeHost(const std::string& message) {
  std::unique_ptr<base::Value> message_value = base::JSONReader::Read(message);
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
  } else if (type == kCRDStateChanged) {
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
  LOG(WARNING) << "Unknown message type :" << type;
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
