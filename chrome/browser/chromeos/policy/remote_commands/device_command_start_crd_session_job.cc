// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_start_crd_session_job.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/policy/remote_commands/crd_logging.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

// OAuth2 Token scopes
constexpr char kCloudDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/clouddevices";
constexpr char kChromotingRemoteSupportOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromoting.remote.support";
constexpr char kTachyonOAuth2Scope[] =
    "https://www.googleapis.com/auth/tachyon";

// Job parameters fields:

// Job requires that UI was idle for at least this period of time
// to proceed. If absent / equal to 0, job will proceed regardless of user
// activity.
const char kIdlenessCutoffFieldName[] = "idlenessCutoffSec";

// Regulates if remote session should be terminated upon any local input event.
const char kTerminateUponInputFieldName[] = "terminateUponInput";

// Result payload fields:

// Integer value containing DeviceCommandStartCRDSessionJob::ResultCode
const char kResultCodeFieldName[] = "resultCode";

// CRD Access Code if job was completed successfully
const char kResultAccessCodeFieldName[] = "accessCode";

// Optional detailed error message for error result codes.
const char kResultMessageFieldName[] = "message";

// Period in seconds since last user activity, if job finished with
// FAILURE_NOT_IDLE result code.
const char kResultLastActivityFieldName[] = "lastActivitySec";

ash::KioskAppManagerBase* GetKioskAppManagerIfKioskAppIsRunning(
    const user_manager::UserManager* user_manager) {
  if (user_manager->IsLoggedInAsKioskApp())
    return ash::KioskAppManager::Get();
  if (user_manager->IsLoggedInAsArcKioskApp())
    return chromeos::ArcKioskAppManager::Get();
  if (user_manager->IsLoggedInAsWebKioskApp())
    return ash::WebKioskAppManager::Get();

  return nullptr;
}

}  // namespace

// Helper class that asynchronously fetches the OAuth token, and passes it to
// the given callback.
class DeviceCommandStartCRDSessionJob::OAuthTokenFetcher
    : public OAuth2AccessTokenManager::Consumer {
 public:
  OAuthTokenFetcher(DeviceOAuth2TokenService& oauth_service,
                    absl::optional<std::string> oauth_token_for_test,
                    OAuthTokenCallback success_callback,
                    ErrorCallback error_callback)
      : OAuth2AccessTokenManager::Consumer("crd_host_delegate"),
        oauth_service_(oauth_service),
        oauth_token_for_test_(std::move(oauth_token_for_test)),
        success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)) {}
  OAuthTokenFetcher(const OAuthTokenFetcher&) = delete;
  OAuthTokenFetcher& operator=(const OAuthTokenFetcher&) = delete;
  ~OAuthTokenFetcher() override = default;

  void Start() {
    CRD_DVLOG(1) << "Fetching OAuth access token";

    if (oauth_token_for_test_) {
      std::move(success_callback_).Run(oauth_token_for_test_.value());
      return;
    }

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
    oauth_request_.reset();
    std::move(success_callback_).Run(token_response.access_token);
  }

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    CRD_DVLOG(1) << "Failed to get OAuth access token: " << error.ToString();
    oauth_request_.reset();
    std::move(error_callback_)
        .Run(DeviceCommandStartCRDSessionJob::FAILURE_NO_OAUTH_TOKEN,
             error.ToString());
  }

  DeviceOAuth2TokenService& oauth_service_;
  absl::optional<std::string> oauth_token_for_test_;
  DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback_;
  DeviceCommandStartCRDSessionJob::ErrorCallback error_callback_;
  // Handler for the OAuth access token request.
  // When deleted the token manager will cancel the request (and not call us).
  std::unique_ptr<OAuth2AccessTokenManager::Request> oauth_request_;
};

class DeviceCommandStartCRDSessionJob::ResultPayload
    : public RemoteCommandJob::ResultPayload {
 public:
  ResultPayload(ResultCode result_code,
                const absl::optional<std::string>& access_code,
                const absl::optional<base::TimeDelta>& time_delta,
                const absl::optional<std::string>& error_message);
  ~ResultPayload() override = default;

  static std::unique_ptr<ResultPayload> CreateSuccessPayload(
      const std::string& access_code);
  static std::unique_ptr<ResultPayload> CreateNonIdlePayload(
      const base::TimeDelta& time_delta);
  static std::unique_ptr<ResultPayload> CreateErrorPayload(
      ResultCode result_code,
      const std::string& error_message);

  // RemoteCommandJob::ResultPayload:
  std::unique_ptr<std::string> Serialize() override;

 private:
  std::string payload_;
};

DeviceCommandStartCRDSessionJob::ResultPayload::ResultPayload(
    ResultCode result_code,
    const absl::optional<std::string>& access_code,
    const absl::optional<base::TimeDelta>& time_delta,
    const absl::optional<std::string>& error_message) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kResultCodeFieldName, base::Value(result_code));
  if (error_message && !error_message.value().empty())
    value.SetKey(kResultMessageFieldName, base::Value(error_message.value()));
  if (access_code)
    value.SetKey(kResultAccessCodeFieldName, base::Value(access_code.value()));
  if (time_delta) {
    value.SetKey(kResultLastActivityFieldName,
                 base::Value(static_cast<int>(time_delta.value().InSeconds())));
  }
  base::JSONWriter::Write(value, &payload_);
}

std::unique_ptr<DeviceCommandStartCRDSessionJob::ResultPayload>
DeviceCommandStartCRDSessionJob::ResultPayload::CreateSuccessPayload(
    const std::string& access_code) {
  return std::make_unique<ResultPayload>(ResultCode::SUCCESS, access_code,
                                         absl::nullopt /*time_delta*/,
                                         absl::nullopt /* error_message */);
}

std::unique_ptr<DeviceCommandStartCRDSessionJob::ResultPayload>
DeviceCommandStartCRDSessionJob::ResultPayload::CreateNonIdlePayload(
    const base::TimeDelta& time_delta) {
  return std::make_unique<ResultPayload>(
      ResultCode::FAILURE_NOT_IDLE, absl::nullopt /* access_code */, time_delta,
      absl::nullopt /* error_message */);
}

std::unique_ptr<DeviceCommandStartCRDSessionJob::ResultPayload>
DeviceCommandStartCRDSessionJob::ResultPayload::CreateErrorPayload(
    ResultCode result_code,
    const std::string& error_message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  DCHECK(result_code != ResultCode::FAILURE_NOT_IDLE);
  return std::make_unique<ResultPayload>(
      result_code, absl::nullopt /* access_code */,
      absl::nullopt /*time_delta*/, error_message);
}

std::unique_ptr<std::string>
DeviceCommandStartCRDSessionJob::ResultPayload::Serialize() {
  return std::make_unique<std::string>(payload_);
}

DeviceCommandStartCRDSessionJob::DeviceCommandStartCRDSessionJob(
    Delegate* crd_host_delegate)
    : delegate_(crd_host_delegate), terminate_session_attemtpted_(false) {}

DeviceCommandStartCRDSessionJob::~DeviceCommandStartCRDSessionJob() = default;

enterprise_management::RemoteCommand_Type
DeviceCommandStartCRDSessionJob::GetType() const {
  return enterprise_management::RemoteCommand_Type_DEVICE_START_CRD_SESSION;
}

void DeviceCommandStartCRDSessionJob::SetOAuthTokenForTest(
    const std::string& token) {
  oauth_token_for_test_ = token;
}

bool DeviceCommandStartCRDSessionJob::ParseCommandPayload(
    const std::string& command_payload) {
  absl::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root)
    return false;
  if (!root->is_dict())
    return false;

  base::Value* idleness_cutoff_value =
      root->FindKeyOfType(kIdlenessCutoffFieldName, base::Value::Type::INTEGER);
  if (idleness_cutoff_value) {
    idleness_cutoff_ =
        base::TimeDelta::FromSeconds(idleness_cutoff_value->GetInt());
  } else {
    idleness_cutoff_ = base::TimeDelta::FromSeconds(0);
  }

  base::Value* terminate_upon_input_value = root->FindKeyOfType(
      kTerminateUponInputFieldName, base::Value::Type::BOOLEAN);
  if (terminate_upon_input_value) {
    terminate_upon_input_ = terminate_upon_input_value->GetBool();
  } else {
    terminate_upon_input_ = false;
  }

  return true;
}

bool DeviceCommandStartCRDSessionJob::AreServicesReady() const {
  return user_manager::UserManager::IsInitialized() &&
         ui::UserActivityDetector::Get() != nullptr &&
         oauth_service() != nullptr;
}

bool DeviceCommandStartCRDSessionJob::IsRunningAutoLaunchedKiosk() const {
  const auto* user_manager = user_manager::UserManager::Get();
  const auto* kiosk_app_manager =
      GetKioskAppManagerIfKioskAppIsRunning(user_manager);

  if (!kiosk_app_manager)
    return false;
  return kiosk_app_manager->current_app_was_auto_launched_with_zero_delay();
}

bool DeviceCommandStartCRDSessionJob::IsDeviceIdle() const {
  return GetDeviceIdlenessPeriod() >= idleness_cutoff_;
}

base::TimeDelta DeviceCommandStartCRDSessionJob::GetDeviceIdlenessPeriod()
    const {
  return base::TimeTicks::Now() -
         ui::UserActivityDetector::Get()->last_activity_time();
}

void DeviceCommandStartCRDSessionJob::FetchOAuthTokenASync(
    OAuthTokenCallback on_success,
    ErrorCallback on_error) {
  DCHECK(!oauth_token_fetcher_ || !oauth_token_fetcher_->is_running());
  DCHECK(oauth_service());

  oauth_token_fetcher_ = std::make_unique<OAuthTokenFetcher>(
      *oauth_service(), std::move(oauth_token_for_test_), std::move(on_success),
      std::move(on_error));
  oauth_token_fetcher_->Start();
}

void DeviceCommandStartCRDSessionJob::FinishWithError(
    const ResultCode result_code,
    const std::string& message) {
  DCHECK(result_code != ResultCode::SUCCESS);
  if (!failed_callback_)
    return;  // Task was terminated.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(failed_callback_),
                     ResultPayload::CreateErrorPayload(result_code, message)));
}

void DeviceCommandStartCRDSessionJob::FinishWithNotIdleError() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(failed_callback_),
                                ResultPayload::CreateNonIdlePayload(
                                    GetDeviceIdlenessPeriod())));
}

void DeviceCommandStartCRDSessionJob::RunImpl(
    CallbackWithResult succeeded_callback,
    CallbackWithResult failed_callback) {
  VLOG(0) << "Running start crd session command";

  if (delegate_->HasActiveSession()) {
    CHECK(!terminate_session_attemtpted_);
    terminate_session_attemtpted_ = true;
    delegate_->TerminateSession(base::BindOnce(
        &DeviceCommandStartCRDSessionJob::RunImpl, weak_factory_.GetWeakPtr(),
        std::move(succeeded_callback), std::move(failed_callback)));
    return;
  }

  terminate_session_attemtpted_ = false;
  failed_callback_ = std::move(failed_callback);
  succeeded_callback_ = std::move(succeeded_callback);

  if (!AreServicesReady()) {
    FinishWithError(ResultCode::FAILURE_SERVICES_NOT_READY, "");
    return;
  }

  if (!IsRunningAutoLaunchedKiosk()) {
    FinishWithError(ResultCode::FAILURE_NOT_A_KIOSK, "");
    return;
  }

  if (!IsDeviceIdle()) {
    FinishWithNotIdleError();
    return;
  }

  FetchOAuthTokenASync(
      /*on_success=*/base::BindOnce(
          &DeviceCommandStartCRDSessionJob::OnOAuthTokenReceived,
          weak_factory_.GetWeakPtr()),
      /*on_error=*/base::BindOnce(
          &DeviceCommandStartCRDSessionJob::FinishWithError,
          weak_factory_.GetWeakPtr()));
}

void DeviceCommandStartCRDSessionJob::OnOAuthTokenReceived(
    const std::string& token) {
  delegate_->StartCRDHostAndGetCode(
      token, GetRobotAccountUserName(), terminate_upon_input_,
      base::BindOnce(&DeviceCommandStartCRDSessionJob::OnAccessCodeReceived,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&DeviceCommandStartCRDSessionJob::FinishWithError,
                     weak_factory_.GetWeakPtr()));
}

void DeviceCommandStartCRDSessionJob::OnAccessCodeReceived(
    const std::string& access_code) {
  if (!succeeded_callback_)
    return;  // Task was terminated.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(succeeded_callback_),
                     ResultPayload::CreateSuccessPayload(access_code)));
}

std::string DeviceCommandStartCRDSessionJob::GetRobotAccountUserName() const {
  CoreAccountId account_id = oauth_service()->GetRobotAccountId();

  // TODO(msarda): This conversion will not be correct once account id is
  // migrated to be the Gaia ID on ChromeOS. Fix it.
  return account_id.ToString();
}

DeviceOAuth2TokenService* DeviceCommandStartCRDSessionJob::oauth_service()
    const {
  return DeviceOAuth2TokenServiceFactory::Get();
}

void DeviceCommandStartCRDSessionJob::TerminateImpl() {
  succeeded_callback_.Reset();
  failed_callback_.Reset();
  weak_factory_.InvalidateWeakPtrs();
  delegate_->TerminateSession(base::OnceClosure());
}

}  // namespace policy
