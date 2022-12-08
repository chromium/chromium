// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_start_crd_session_job.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/policy/remote_commands/fake_cros_network_config_base.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "extensions/common/value_builder.h"
#include "remoting/host/chromeos/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace policy {

namespace {

using ::base::test::RepeatingTestFuture;
using ::base::test::TestFuture;
using extensions::DictionaryBuilder;
using ResultCode = DeviceCommandStartCrdSessionJob::ResultCode;
using UmaSessionType = DeviceCommandStartCrdSessionJob::UmaSessionType;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::NetworkTypeStateProperties;
using chromeos::network_config::mojom::NetworkTypeStatePropertiesPtr;
using chromeos::network_config::mojom::OncSource;
using remoting::features::kEnableCrdAdminRemoteAccess;

namespace em = ::enterprise_management;

constexpr char kResultCodeFieldName[] = "resultCode";
constexpr char kResultMessageFieldName[] = "message";
constexpr char kResultAccessCodeFieldName[] = "accessCode";
constexpr char kResultLastActivityFieldName[] = "lastActivitySec";

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

constexpr char kTestOAuthToken[] = "test-oauth-token";
constexpr char kTestAccessCode[] = "111122223333";
constexpr char kTestNoOAuthTokenReason[] = "Not authorized.";
constexpr char kTestAccountEmail[] = "test.account.email@example.com";

// Macro expecting success. We are using a macro because a function would
// report any error against the line in the function, and not against the
// place where EXPECT_SUCCESS is called.
#define EXPECT_SUCCESS(statement_)                                          \
  ({                                                                        \
    auto result_ = statement_;                                              \
    EXPECT_EQ(result_.status, RemoteCommandJob::Status::SUCCEEDED);         \
    EXPECT_THAT(result_.payload,                                            \
                base::test::IsJson(CreateSuccessPayload(kTestAccessCode))); \
  })

// Macro expecting error. We are using a macro because a function would
// report any error against the line in the function, and not against the
// place where EXPECT_ERROR is called.
#define EXPECT_ERROR(statement_, error_code, ...)                       \
  ({                                                                    \
    auto result_ = statement_;                                          \
    EXPECT_EQ(result_.status, RemoteCommandJob::Status::FAILED);        \
    EXPECT_THAT(result_.payload, base::test::IsJson(CreateErrorPayload( \
                                     error_code, ##__VA_ARGS__)));      \
  })

em::RemoteCommand GenerateCommandProto(RemoteCommandJob::UniqueIDType unique_id,
                                       base::TimeDelta age_of_command,
                                       std::string payload) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_DEVICE_START_CRD_SESSION);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  command_proto.set_payload(payload);
  return command_proto;
}

class StubCrdHostDelegate : public DeviceCommandStartCrdSessionJob::Delegate {
 public:
  StubCrdHostDelegate() = default;
  ~StubCrdHostDelegate() override = default;

  void SetHasActiveSession(bool value) { has_active_session_ = value; }
  void MakeAccessCodeFetchFail() { access_code_success_ = false; }

  // Returns if TerminateSession() was called to terminate the active session.
  bool IsActiveSessionTerminated() const { return terminate_session_called_; }

  // Returns the |SessionParameters| sent to the last StartCrdHostAndGetCode()
  // call.
  SessionParameters session_parameters() {
    EXPECT_TRUE(received_session_parameters_.has_value());
    return received_session_parameters_.value_or(SessionParameters{});
  }

  // DeviceCommandStartCrdSessionJob::Delegate implementation:
  bool HasActiveSession() const override;
  void TerminateSession(base::OnceClosure callback) override;
  void StartCrdHostAndGetCode(
      const SessionParameters& parameters,
      DeviceCommandStartCrdSessionJob::AccessCodeCallback success_callback,
      DeviceCommandStartCrdSessionJob::ErrorCallback error_callback) override;

 private:
  bool has_active_session_ = false;
  bool access_code_success_ = true;
  bool terminate_session_called_ = false;
  absl::optional<SessionParameters> received_session_parameters_;
};

bool StubCrdHostDelegate::HasActiveSession() const {
  return has_active_session_;
}

void StubCrdHostDelegate::TerminateSession(base::OnceClosure callback) {
  has_active_session_ = false;
  terminate_session_called_ = true;
  std::move(callback).Run();
}

void StubCrdHostDelegate::StartCrdHostAndGetCode(
    const SessionParameters& parameters,
    DeviceCommandStartCrdSessionJob::AccessCodeCallback success_callback,
    DeviceCommandStartCrdSessionJob::ErrorCallback error_callback) {
  received_session_parameters_ = parameters;

  if (access_code_success_) {
    std::move(success_callback).Run(kTestAccessCode);
  } else {
    std::move(error_callback)
        .Run(ResultCode::FAILURE_CRD_HOST_ERROR, std::string());
  }
}

struct Result {
  RemoteCommandJob::Status status;
  std::string payload;
};

// Helper class that creates a `NetworkStatePropertiesPtr`.
class NetworkBuilder {
 public:
  explicit NetworkBuilder(NetworkType type) {
    network_->type = type;
    network_->guid = "<network-guid>";
    network_->type_state = CreateTypeStateForType(type);
  }
  NetworkBuilder(NetworkBuilder&& other) = default;
  NetworkBuilder& operator=(NetworkBuilder&&) = default;

  NetworkBuilder(const NetworkBuilder& other)
      : network_(other.network_.Clone()) {}
  void operator=(const NetworkBuilder& other) {
    this->network_ = other.network_.Clone();
  }

  ~NetworkBuilder() = default;

  NetworkBuilder& SetOncSource(OncSource source) {
    network_->source = source;
    return *this;
  }

  NetworkStatePropertiesPtr Build() const { return network_.Clone(); }

 private:
  NetworkTypeStatePropertiesPtr CreateTypeStateForType(NetworkType type) {
    switch (type) {
      case NetworkType::kCellular:
        return NetworkTypeStateProperties::NewCellular(
            chromeos::network_config::mojom::CellularStateProperties::New());
      case NetworkType::kEthernet:
        return NetworkTypeStateProperties::NewEthernet(
            chromeos::network_config::mojom::EthernetStateProperties::New());
      case NetworkType::kTether:
        return NetworkTypeStateProperties::NewTether(
            chromeos::network_config::mojom::TetherStateProperties::New());
      case NetworkType::kVPN:
        return NetworkTypeStateProperties::NewVpn(
            chromeos::network_config::mojom::VPNStateProperties::New());
      case NetworkType::kWiFi:
        return NetworkTypeStateProperties::NewWifi(
            chromeos::network_config::mojom::WiFiStateProperties::New());
      case NetworkType::kAll:
      case NetworkType::kMobile:
      case NetworkType::kWireless:
        // These are not actual network types, but just shorthands used while
        // filtering.
        NOTREACHED();
        break;
    }
    NOTREACHED();
    return nullptr;
  }
  NetworkStatePropertiesPtr network_ = NetworkStateProperties::New();
};

NetworkBuilder CreateNetwork(NetworkType type = NetworkType::kWiFi) {
  return NetworkBuilder(type);
}

// Fake implementation of `CrosNetworkConfig` that simply stores a list of
// networks that will be returned on each `GetNetworkStateList()` call.
class FakeCrosNetworkConfig : public FakeCrosNetworkConfigBase {
 public:
  FakeCrosNetworkConfig() = default;
  FakeCrosNetworkConfig(const FakeCrosNetworkConfig&) = delete;
  FakeCrosNetworkConfig& operator=(const FakeCrosNetworkConfig&) = delete;
  ~FakeCrosNetworkConfig() override = default;

  void SetActiveNetworks(std::vector<NetworkBuilder> networks) {
    active_networks_.clear();
    for (auto& builder : networks) {
      active_networks_.push_back(builder.Build());
    }
  }

  void AddActiveNetwork(NetworkBuilder builder) {
    active_networks_.push_back(builder.Build());
  }

  void ClearActiveNetworks() { active_networks_.clear(); }

  // `FakeCrosNetworkConfigBase` implementation:
  void GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilterPtr filter,
      GetNetworkStateListCallback callback) override {
    std::vector<NetworkStatePropertiesPtr> networks;
    for (const auto& network : active_networks_) {
      networks.push_back(network->Clone());
    }

    std::move(callback).Run(std::move(networks));
  }

 private:
  std::vector<NetworkStatePropertiesPtr> active_networks_;
};

class MockCrosNetworkConfig : public FakeCrosNetworkConfigBase {
 public:
  MockCrosNetworkConfig() = default;
  MockCrosNetworkConfig(const MockCrosNetworkConfig&) = delete;
  MockCrosNetworkConfig& operator=(const MockCrosNetworkConfig&) = delete;
  ~MockCrosNetworkConfig() override = default;

  MOCK_METHOD(void,
              GetNetworkStateList,
              (chromeos::network_config::mojom::NetworkFilterPtr filter,
               GetNetworkStateListCallback callback));
};

}  // namespace

class DeviceCommandStartCrdSessionJobTest : public ash::DeviceSettingsTestBase {
 public:
  DeviceCommandStartCrdSessionJobTest()
      : ash::DeviceSettingsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // ash::DeviceSettingsTestBase implementation:
  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    user_activity_detector_ = std::make_unique<ui::UserActivityDetector>();
    arc_kiosk_app_manager_ = std::make_unique<ash::ArcKioskAppManager>();
    web_kiosk_app_manager_ = std::make_unique<ash::WebKioskAppManager>();

    // SystemSaltGetter is used by the token service.
    chromeos::SystemSaltGetter::Initialize();
    DeviceOAuth2TokenServiceFactory::Initialize(
        test_url_loader_factory_.GetSafeWeakWrapper(), &local_state_);
    RegisterLocalState(local_state_.registry());

    chromeos::network_config::OverrideInProcessInstanceForTesting(
        &fake_cros_network_config_);
  }

  void TearDown() override {
    chromeos::network_config::OverrideInProcessInstanceForTesting(nullptr);
    DeviceOAuth2TokenServiceFactory::Shutdown();
    chromeos::SystemSaltGetter::Shutdown();

    web_kiosk_app_manager_.reset();
    arc_kiosk_app_manager_.reset();

    DeviceSettingsTestBase::TearDown();
  }

  Result RunJobAndWaitForResult(
      const DictionaryBuilder& payload = DictionaryBuilder()) {
    bool launched = InitializeAndRunJob(payload);
    EXPECT_TRUE(launched) << "Failed to launch the job";
    // Do not wait for the result if the job was never launched in the first
    // place.
    if (!launched)
      return Result{RemoteCommandJob::Status::NOT_INITIALIZED};

    return future_result_.Take();
  }

  // Create an empty payload builder.
  DictionaryBuilder Payload() const { return DictionaryBuilder(); }

  std::string CreateSuccessPayload(const std::string& access_code);
  std::string CreateErrorPayload(ResultCode result_code,
                                 const std::string& error_message);
  std::string CreateNotIdlePayload(int idle_time_in_sec);

  void LogInAsManagedGuestSessionUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddPublicAccountUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsRegularUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsAffiliatedUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddUserWithAffiliation(account_id, /*is_affiliated=*/true);
    user_manager().LoginUser(account_id);
  }

  void LogInAsGuestUser() {
    const user_manager::User* user = user_manager().AddGuestUser();
    user_manager().LoginUser(user->GetAccountId());
  }

  void LogInAsKioskAppUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddKioskAppUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsArcKioskAppUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddArcKioskAppUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsWebKioskAppUser() {
    const AccountId account_id(AccountId::FromUserEmail(kTestAccountEmail));

    user_manager().AddWebKioskAppUser(account_id);
    user_manager().LoginUser(account_id);
  }

  void LogInAsAutoLaunchedKioskAppUser() {
    LogInAsKioskAppUser();
    ash::KioskAppManager::Get()
        ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);
  }

  void LogInAsManuallyLaunchedKioskAppUser() {
    LogInAsKioskAppUser();
    ash::KioskAppManager::Get()
        ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);
  }

  void SetDeviceIdleTime(int idle_time_in_sec) {
    user_activity_detector_->set_last_activity_time_for_test(
        base::TimeTicks::Now() - base::Seconds(idle_time_in_sec));
  }

  void SetOAuthToken(std::string value) { oauth_token_ = value; }

  void SetRobotAccountUserName(const std::string& user_name) {
    DeviceOAuth2TokenService* token_service =
        DeviceOAuth2TokenServiceFactory::Get();
    token_service->set_robot_account_id_for_testing(CoreAccountId(user_name));
  }

  void ClearOAuthToken() { oauth_token_ = absl::nullopt; }

  StubCrdHostDelegate& crd_host_delegate() { return crd_host_delegate_; }
  DeviceCommandStartCrdSessionJob& job() {
    DCHECK(job_);
    return *job_;
  }

  bool InitializeJob(const DictionaryBuilder& payload) {
    job_ =
        std::make_unique<DeviceCommandStartCrdSessionJob>(&crd_host_delegate_);

    bool success = job().Init(
        base::TimeTicks::Now(),
        GenerateCommandProto(kUniqueID, base::TimeDelta(), payload.ToJSON()),
        em::SignedData());

    if (oauth_token_)
      job().SetOAuthTokenForTest(oauth_token_.value());

    if (success) {
      EXPECT_EQ(kUniqueID, job().unique_id());
      EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job().status());
    }
    return success;
  }

  // Initialize and run the remote command job.
  // The result will be stored in |future_result_|.
  bool InitializeAndRunJob(const DictionaryBuilder& payload) {
    bool success = InitializeJob(payload);
    EXPECT_TRUE(success) << "Failed to initialize the job";
    if (!success)
      return false;

    bool launched = job().Run(
        base::Time::Now(), base::TimeTicks::Now(),
        base::BindOnce(&DeviceCommandStartCrdSessionJobTest::OnJobFinished,
                       base::Unretained(this)));
    return launched;
  }

  FakeCrosNetworkConfig& fake_cros_network_config() {
    return fake_cros_network_config_;
  }

 private:
  ash::FakeChromeUserManager& user_manager() { return *user_manager_; }

  // Callback invoked when the remote command job finished.
  void OnJobFinished() {
    std::string payload =
        job().GetResultPayload() ? *job().GetResultPayload() : "<nullptr>";

    future_result_.AddValue(Result{job().status(), payload});
  }

  std::unique_ptr<ash::ArcKioskAppManager> arc_kiosk_app_manager_;
  std::unique_ptr<ash::WebKioskAppManager> web_kiosk_app_manager_;

  absl::optional<std::string> oauth_token_ = kTestOAuthToken;

  // Automatically installed as a singleton upon creation.
  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple local_state_;

  StubCrdHostDelegate crd_host_delegate_;
  std::unique_ptr<DeviceCommandStartCrdSessionJob> job_;

  FakeCrosNetworkConfig fake_cros_network_config_;

  // Future value that will be populated with the result once the remote
  // command job is completed.
  RepeatingTestFuture<Result> future_result_;
};

std::string DeviceCommandStartCrdSessionJobTest::CreateSuccessPayload(
    const std::string& access_code) {
  return DictionaryBuilder()
      .Set(kResultCodeFieldName, static_cast<int>(ResultCode::SUCCESS))
      .Set(kResultAccessCodeFieldName, access_code)
      .ToJSON();
}

std::string DeviceCommandStartCrdSessionJobTest::CreateErrorPayload(
    ResultCode result_code,
    const std::string& error_message = "") {
  DictionaryBuilder builder;
  builder.Set(kResultCodeFieldName, static_cast<int>(result_code));
  if (!error_message.empty())
    builder.Set(kResultMessageFieldName, error_message);
  return builder.ToJSON();
}

std::string DeviceCommandStartCrdSessionJobTest::CreateNotIdlePayload(
    int idle_time_in_sec) {
  return DictionaryBuilder()
      .Set(kResultCodeFieldName, static_cast<int>(ResultCode::FAILURE_NOT_IDLE))
      .Set(kResultLastActivityFieldName, idle_time_in_sec)
      .ToJSON();
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedIfAccessTokenCanBeFetched) {
  LogInAsAutoLaunchedKioskAppUser();

  SetOAuthToken(kTestOAuthToken);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldTerminateActiveSessionAndThenSucceed) {
  LogInAsAutoLaunchedKioskAppUser();

  crd_host_delegate().SetHasActiveSession(true);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_TRUE(crd_host_delegate().IsActiveSessionTerminated());
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailForGuestUser) {
  LogInAsGuestUser();

  EXPECT_ERROR(RunJobAndWaitForResult(),
               ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailForRegularUser) {
  LogInAsRegularUser();

  EXPECT_ERROR(RunJobAndWaitForResult(),
               ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForManuallyLaunchedKioskUser) {
  LogInAsKioskAppUser();

  ash::KioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForAutoLaunchedKioskUser) {
  LogInAsKioskAppUser();
  ash::KioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForManuallyLaunchedArcKioskUser) {
  SetOAuthToken(kTestOAuthToken);

  LogInAsArcKioskAppUser();
  ash::ArcKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForAutoLaunchedArcKioskUser) {
  LogInAsArcKioskAppUser();
  ash::ArcKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForManuallyLaunchedWebKioskUser) {
  LogInAsWebKioskAppUser();
  ash::WebKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(false);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedForAutoLaunchedWebKioskUser) {
  LogInAsWebKioskAppUser();
  ash::WebKioskAppManager::Get()
      ->set_current_app_was_auto_launched_with_zero_delay_for_testing(true);

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfDeviceIdleTimeIsLessThanIdlenessCutoffValue) {
  LogInAsAutoLaunchedKioskAppUser();

  const int device_idle_time_in_sec = 9;
  const int idleness_cutoff_in_sec = 10;

  SetDeviceIdleTime(device_idle_time_in_sec);

  Result result = RunJobAndWaitForResult(
      Payload().Set("idlenessCutoffSec", idleness_cutoff_in_sec));
  EXPECT_EQ(result.status, RemoteCommandJob::Status::FAILED);
  EXPECT_EQ(result.payload, CreateNotIdlePayload(device_idle_time_in_sec));
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSucceedIfDeviceIdleTimeIsMoreThanIdlenessCutoffValue) {
  LogInAsAutoLaunchedKioskAppUser();

  const int device_idle_time_in_sec = 10;
  const int idleness_cutoff_in_sec = 9;

  SetDeviceIdleTime(device_idle_time_in_sec);

  EXPECT_SUCCESS(RunJobAndWaitForResult(
      Payload().Set("idlenessCutoffSec", idleness_cutoff_in_sec)));
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldCheckUserTypeBeforeDeviceIdleTime) {
  // If we were to check device idle time first, the remote admin would
  // still be asked to acknowledge the user's presence, even if they are not
  // allowed to start a CRD connection anyway.
  LogInAsRegularUser();

  const int device_idle_time_in_sec = 9;
  const int idleness_cutoff_in_sec = 10;

  SetDeviceIdleTime(device_idle_time_in_sec);

  EXPECT_ERROR(RunJobAndWaitForResult(
                   Payload().Set("idlenessCutoffSec", idleness_cutoff_in_sec)),
               ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldFailIfWeCantFetchTheOAuthToken) {
  LogInAsAutoLaunchedKioskAppUser();
  ClearOAuthToken();

  EXPECT_ERROR(RunJobAndWaitForResult(), ResultCode::FAILURE_NO_OAUTH_TOKEN,
               kTestNoOAuthTokenReason);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailIfCrdHostReportsAnError) {
  LogInAsAutoLaunchedKioskAppUser();

  crd_host_delegate().MakeAccessCodeFetchFail();

  EXPECT_ERROR(RunJobAndWaitForResult(), ResultCode::FAILURE_CRD_HOST_ERROR);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldPassOAuthTokenToDelegate) {
  LogInAsAutoLaunchedKioskAppUser();
  SetOAuthToken("the-oauth-token");

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_EQ("the-oauth-token",
            crd_host_delegate().session_parameters().oauth_token);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassRobotAccountNameToDelegate) {
  LogInAsAutoLaunchedKioskAppUser();

  SetRobotAccountUserName("robot-account");

  EXPECT_SUCCESS(RunJobAndWaitForResult());

  EXPECT_EQ("robot-account",
            crd_host_delegate().session_parameters().user_name);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputTrueToDelegateForAutolaunchedKioskIfAckedUserPresenceSetFalse) {
  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("acked_user_presence", false)));

  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputFalseToDelegateForAutolaunchedKioskIfAckedUserPresenceSetTrue) {
  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("ackedUserPresence", true)));

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputTrueToDelegateForManuallylaunchedKioskIfAckedUserPresenceSetFalse) {
  LogInAsManuallyLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("acked_user_presence", false)));

  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(
    DeviceCommandStartCrdSessionJobTest,
    ShouldPassTerminateUponInputFalseToDelegateForManuallyLaunchedKioskIfAckedUserPresenceSetTrue) {
  LogInAsManuallyLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("ackedUserPresence", true)));

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassShowConfirmationDialogFalseToDelegateForKioskUser) {
  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());

  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldFailIfNoUserIsLoggedIn) {
  EXPECT_ERROR(RunJobAndWaitForResult(),
               ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldSucceedForManagedGuestUser) {
  LogInAsManagedGuestSessionUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest, ShouldSucceedForAffiliatedUser) {
  LogInAsAffiliatedUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassShowConfirmationDialogTrueToDelegateForManagedGuestUser) {
  LogInAsManagedGuestSessionUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldPassShowConfirmationDialogTrueToDelegateForAffiliatedUser) {
  LogInAsAffiliatedUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_EQ(true,
            crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldNeverSendTerminateUponInputTrueToDelegateForAffiliatedUser) {
  LogInAsAffiliatedUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("ackedUserPresense", false)));
  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldNeverSendTerminateUponInputTrueToDelegateForManagedGuestUser) {
  LogInAsManagedGuestSessionUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("ackedUserPresense", false)));
  EXPECT_EQ(false,
            crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenAutoLaunchedKioskConnects) {
  base::HistogramTester histogram_tester;

  LogInAsAutoLaunchedKioskAppUser();
  crd_host_delegate().SetHasActiveSession(true);
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kAutoLaunchedKiosk, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenManuallyLaunchedKioskConnects) {
  base::HistogramTester histogram_tester;

  LogInAsManuallyLaunchedKioskAppUser();
  crd_host_delegate().SetHasActiveSession(true);
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kManuallyLaunchedKiosk, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenAffiliatedUserConnects) {
  base::HistogramTester histogram_tester;

  LogInAsAffiliatedUser();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kAffiliatedUser, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendSuccessUmaLogWhenManagedGuestSessionConnects) {
  base::HistogramTester histogram_tester;

  LogInAsManagedGuestSessionUser();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::SUCCESS, 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.SessionType",
      UmaSessionType::kManagedGuestSession, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogWhenUserTypeIsNotSupported) {
  base::HistogramTester histogram_tester;

  LogInAsRegularUser();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_UNSUPPORTED_USER_TYPE, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogWhenDeviceIsNotIdle) {
  base::HistogramTester histogram_tester;
  LogInAsAutoLaunchedKioskAppUser();

  const int device_idle_time_in_sec = 9;
  const int idleness_cutoff_in_sec = 10;

  SetDeviceIdleTime(device_idle_time_in_sec);
  RunJobAndWaitForResult(
      Payload().Set("idlenessCutoffSec", idleness_cutoff_in_sec));

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result", ResultCode::FAILURE_NOT_IDLE,
      1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogFailureNoAuthToken) {
  base::HistogramTester histogram_tester;
  LogInAsAffiliatedUser();

  ClearOAuthToken();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_NO_OAUTH_TOKEN, 1);
}

TEST_F(DeviceCommandStartCrdSessionJobTest,
       ShouldSendErrorUmaLogFailureCrdHostError) {
  base::HistogramTester histogram_tester;
  LogInAsAutoLaunchedKioskAppUser();

  crd_host_delegate().MakeAccessCodeFetchFail();
  RunJobAndWaitForResult();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceRemoteCommand.Crd.Result",
      ResultCode::FAILURE_CRD_HOST_ERROR, 1);
}

class DeviceCommandStartCrdSessionJobCurtainSessionTest
    : public DeviceCommandStartCrdSessionJobTest {
 public:
  void SetUp() override {
    EnableFeature(kEnableCrdAdminRemoteAccess);
    DeviceCommandStartCrdSessionJobTest::SetUp();
  }

  void EnableFeature(const base::Feature& feature) {
    feature_.Reset();
    feature_.InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    feature_.Reset();
    feature_.InitAndDisableFeature(feature);
  }

  void AddActiveManagedNetwork() {
    fake_cros_network_config().AddActiveNetwork(
        CreateNetwork(NetworkType::kWiFi)
            .SetOncSource(OncSource::kDevicePolicy));
  }

 private:
  base::test::ScopedFeatureList feature_;
};

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldUseCurtainLocalUserSessionFalseIfFeatureIsDisabled) {
  DisableFeature(kEnableCrdAdminRemoteAccess);

  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult());
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().curtain_local_user_session);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldDefaultCurtainLocalUserSessionToFalseIfUnspecifiedInPayload) {
  AddActiveManagedNetwork();
  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(RunJobAndWaitForResult(Payload()));
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().curtain_local_user_session);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldRejectCurtainLocalUserSessionTrueInPayloadIfFeatureIsDisabled) {
  DisableFeature(kEnableCrdAdminRemoteAccess);

  bool success = InitializeJob(Payload().Set("curtainLocalUserSession", true));

  EXPECT_FALSE(success);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForGuestUser) {
  AddActiveManagedNetwork();

  LogInAsGuestUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForManagedGuestSessionUser) {
  AddActiveManagedNetwork();

  LogInAsManagedGuestSessionUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForRegularUser) {
  AddActiveManagedNetwork();

  LogInAsRegularUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForAffiliatedUser) {
  AddActiveManagedNetwork();

  LogInAsAffiliatedUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForKioskUserWithoutAutoLaunch) {
  AddActiveManagedNetwork();

  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldFailForKioskUserWithAutoLaunch) {
  AddActiveManagedNetwork();

  LogInAsKioskAppUser();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      ResultCode::FAILURE_UNSUPPORTED_USER_TYPE);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldSucceedIfNoUserIsLoggedIn) {
  AddActiveManagedNetwork();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldSetCurtainLocalUserSessionTrue) {
  AddActiveManagedNetwork();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
  EXPECT_TRUE(
      crd_host_delegate().session_parameters().curtain_local_user_session);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldSetCurtainLocalUserSessionFalse) {
  AddActiveManagedNetwork();

  LogInAsAutoLaunchedKioskAppUser();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", false)));
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().curtain_local_user_session);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldNotTerminateUponInput) {
  AddActiveManagedNetwork();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload()
                                 .Set("curtainLocalUserSession", true)
                                 // This would enable terminate upon input in
                                 // a non-curtained job.
                                 .Set("ackedUserPresense", false)));
  EXPECT_FALSE(crd_host_delegate().session_parameters().terminate_upon_input);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldNotShowConfirmationDialog) {
  AddActiveManagedNetwork();

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
  EXPECT_FALSE(
      crd_host_delegate().session_parameters().show_confirmation_dialog);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldRejectRequestIfThereAreNoActiveNetworks) {
  fake_cros_network_config().ClearActiveNetworks();

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      ResultCode::FAILURE_UNMANAGED_ENVIRONMENT);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldRejectRequestIfOnlyUnmanagedNetworksAreAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kWiFi).SetOncSource(OncSource::kNone),
      CreateNetwork(NetworkType::kEthernet).SetOncSource(OncSource::kNone),
      CreateNetwork(NetworkType::kTether).SetOncSource(OncSource::kNone),
      CreateNetwork(NetworkType::kVPN).SetOncSource(OncSource::kNone),
  });

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      ResultCode::FAILURE_UNMANAGED_ENVIRONMENT);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldRejectRequestIfTheOnlyManagedNetworkIsCellular) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kCellular)
          .SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_ERROR(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)),
      ResultCode::FAILURE_UNMANAGED_ENVIRONMENT);
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldAllowRequestIfManagedWifiNetworkIsAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kWiFi).SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldAllowRequestIfManagedEthernetNetworkIsAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kEthernet)
          .SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldAllowRequestIfManagedTetherNetworkIsAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kTether)
          .SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldAllowRequestIfManagedVpnNetworkIsAvailable) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork(NetworkType::kVPN).SetOncSource(OncSource::kDevicePolicy),
  });

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldNotOnlyLookAtFirstNetwork) {
  fake_cros_network_config().SetActiveNetworks({
      CreateNetwork().SetOncSource(OncSource::kNone),
      CreateNetwork().SetOncSource(OncSource::kDevicePolicy),
      CreateNetwork().SetOncSource(OncSource::kNone),
  });

  EXPECT_SUCCESS(
      RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true)));
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldOnlyAllowPolicyOncSources) {
  for (auto [source, is_allowed] : {
           std::make_pair(OncSource::kNone, /*is_allowed=*/false),
           std::make_pair(OncSource::kDevice, /*is_allowed=*/false),
           std::make_pair(OncSource::kUser, /*is_allowed=*/false),
           std::make_pair(OncSource::kDevicePolicy, /*is_allowed=*/true),
           std::make_pair(OncSource::kUserPolicy, /*is_allowed=*/true),
       }) {
    fake_cros_network_config().SetActiveNetworks({
        CreateNetwork().SetOncSource(source),
    });

    auto expected_result = is_allowed ? RemoteCommandJob::Status::SUCCEEDED
                                      : RemoteCommandJob::Status::FAILED;
    auto actual_result =
        RunJobAndWaitForResult(Payload().Set("curtainLocalUserSession", true))
            .status;
    EXPECT_EQ(actual_result, expected_result);
  }
}

TEST_F(DeviceCommandStartCrdSessionJobCurtainSessionTest,
       ShouldOnlyFetchTheActiveNetworks) {
  MockCrosNetworkConfig network_config_mock;
  chromeos::network_config::OverrideInProcessInstanceForTesting(
      &network_config_mock);

  TestFuture<chromeos::network_config::mojom::NetworkFilterPtr,
             MockCrosNetworkConfig::GetNetworkStateListCallback>
      get_network_state_future;
  EXPECT_CALL(network_config_mock, GetNetworkStateList)
      .WillOnce([&](auto filter, auto callback) {
        get_network_state_future.SetValue(std::move(filter),
                                          std::move(callback));
      });

  InitializeAndRunJob(Payload().Set("curtainLocalUserSession", true));

  auto [filter, callback] = get_network_state_future.Take();
  EXPECT_EQ(filter->filter,
            chromeos::network_config::mojom::FilterType::kActive);
  EXPECT_EQ(filter->network_type,
            chromeos::network_config::mojom::NetworkType::kAll);
  EXPECT_EQ(filter->limit, chromeos::network_config::mojom::kNoLimit);

  // We must invoke the callback to satisfy the Mojom contract
  std::move(callback).Run({});
}

}  // namespace policy
