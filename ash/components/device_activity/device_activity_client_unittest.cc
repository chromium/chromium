// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

#include "ash/components/device_activity/device_activity_controller.h"
#include "ash/components/device_activity/fresnel_pref_names.h"
#include "ash/components/device_activity/fresnel_service.pb.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Holds data used to create deterministic PSM network request/response protos.
struct PsmTestData {
  psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase test_case;
  std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;
  FresnelPsmRlweOprfResponse fresnel_oprf_response;
  FresnelPsmRlweQueryResponse fresnel_query_response;
};

PsmTestData* GetPsmTestData() {
  static base::NoDestructor<PsmTestData> data;
  return data.get();
}

// TODO(https://crbug.com/1272922): Move shared configuration constants to
// separate file.
//
// URLs for the different network requests being performed.
const char kTestFresnelBaseUrl[] = "https://dummy.googleapis.com";
const char kPsmImportRequestEndpoint[] = "/v1/fresnel/psmRlweImport";

// Create fake secrets used by the |DeviceActivityClient|.
constexpr char kFakePsmDeviceActiveSecret[] = "FAKE_PSM_DEVICE_ACTIVE_SECRET";
constexpr char kFakeFresnelApiKey[] = "FAKE_FRESNEL_API_KEY";

// Number of test cases exist in cros_test_data.binarypb file, which is part of
// private_membership third_party library.
const int kNumberOfPsmTestCases = 10;

// PrivateSetMembership regression tests maximum file size which is 4MB.
const size_t kMaxFileSizeInBytes = 4 * (1 << 20);

std::string GetFresnelTestEndpoint(const std::string& endpoint) {
  return kTestFresnelBaseUrl + endpoint;
}

bool ParseProtoFromFile(const base::FilePath& file_path,
                        google::protobuf::MessageLite* out_proto) {
  if (!out_proto)
    return false;

  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_content,
                                         kMaxFileSizeInBytes)) {
    return false;
  }
  return out_proto->ParseFromString(file_content);
}

base::TimeDelta TimeUntilNextUTCMidnight() {
  const auto now = base::Time::Now();
  return (now.UTCMidnight() + base::Hours(base::Time::kHoursPerDay) - now);
}

}  // namespace

class FakePsmDelegate : public PsmDelegate {
 public:
  FakePsmDelegate(const std::string& ec_cipher_key,
                  const std::string& seed,
                  const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids)
      : ec_cipher_key_(ec_cipher_key),
        seed_(seed),
        plaintext_ids_(plaintext_ids) {}
  FakePsmDelegate(const FakePsmDelegate&) = delete;
  FakePsmDelegate& operator=(const FakePsmDelegate&) = delete;
  ~FakePsmDelegate() override = default;

  // PsmDelegate:
  rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  CreatePsmClient(private_membership::rlwe::RlweUseCase use_case,
                  const std::vector<private_membership::rlwe::RlwePlaintextId>&
                      plaintext_ids) override {
    return psm_rlwe::PrivateMembershipRlweClient::CreateForTesting(
        use_case, plaintext_ids_, ec_cipher_key_, seed_);
  }

 private:
  // Used by the PSM client to generate deterministic request/response protos.
  std::string ec_cipher_key_;
  std::string seed_;
  std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids_;
};

class DeviceActivityClientTest : public testing::Test {
 public:
  DeviceActivityClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Start base::Time::Now() at least after epoch time by forwarding 24h.
    // DeviceActivityClient assumes epoch as a date in the past.
    // Remote env. runs unit tests assuming base::Time::Now() is epoch.
    task_environment_.FastForwardBy(base::Hours(base::Time::kHoursPerDay));
    task_environment_.RunUntilIdle();
  }
  DeviceActivityClientTest(const DeviceActivityClientTest&) = delete;
  DeviceActivityClientTest& operator=(const DeviceActivityClientTest&) = delete;
  ~DeviceActivityClientTest() override = default;

 protected:
  static void SetUpTestSuite() {
    // Initialize |psm_test_case_| which is used to generate deterministic psm
    // protos.
    CreatePsmTestCase();

    PsmTestData* psm_test_data = GetPsmTestData();

    // Return well formed plaintext ids used in faking PSM network requests.
    std::vector<psm_rlwe::RlwePlaintextId> fake_plaintext_ids{
        psm_test_data->test_case.plaintext_id()};
    psm_test_data->plaintext_ids = std::move(fake_plaintext_ids);

    // Initialize well formed Oprf and Query response body used to
    // deterministically fake PSM network responses.
    *psm_test_data->fresnel_oprf_response.mutable_rlwe_oprf_response() =
        psm_test_data->test_case.oprf_response();
    *psm_test_data->fresnel_query_response.mutable_rlwe_query_response() =
        psm_test_data->test_case.query_response();
  }

  static void CreatePsmTestCase() {
    base::FilePath src_root_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
    const base::FilePath kPsmTestDataPath =
        src_root_dir.AppendASCII("third_party")
            .AppendASCII("private_membership")
            .AppendASCII("src")
            .AppendASCII("internal")
            .AppendASCII("testing")
            .AppendASCII("regression_test_data")
            .AppendASCII("cros_test_data.binarypb");
    ASSERT_TRUE(base::PathExists(kPsmTestDataPath));
    psm_rlwe::PrivateMembershipRlweClientRegressionTestData test_data;
    ASSERT_TRUE(ParseProtoFromFile(kPsmTestDataPath, &test_data));

    // Note that the test cases can change since it's read from the binarypb.
    // This can cause unexpected failures for the unit tests below.
    // As a safety precaution, check whether the number of tests change.
    ASSERT_EQ(test_data.test_cases_size(), kNumberOfPsmTestCases);

    // Sets |psm_test_case_| to have one of the fake PSM request/response
    // protos.
    GetPsmTestData()->test_case = test_data.test_cases(0);
  }

  // testing::Test:
  void SetUp() override {
    // Initialize pointer to our fake |PsmTestData| object.
    psm_test_data_ = GetPsmTestData();

    network_state_test_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);

    CreateWifiNetworkConfig();

    // Initialize |local_state_| prefs used by device_activity_client class.
    DeviceActivityController::RegisterPrefs(local_state_.registry());
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    device_activity_client_ = std::make_unique<DeviceActivityClient>(
        network_state_test_helper_->network_state_handler(), &local_state_,
        test_shared_loader_factory_,
        std::make_unique<FakePsmDelegate>(
            psm_test_data_->test_case.ec_cipher_key(),
            psm_test_data_->test_case.seed(),
            std::move(psm_test_data_->plaintext_ids)),
        std::make_unique<base::MockRepeatingTimer>(), kTestFresnelBaseUrl,
        kFakeFresnelApiKey, kFakePsmDeviceActiveSecret);
  }

  void TearDown() override {}

  void CreateWifiNetworkConfig() {
    ASSERT_TRUE(wifi_network_service_path_.empty());

    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \""
       << "wifi_guid"
       << "\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateOffline << "\""
       << "}";

    wifi_network_service_path_ =
        network_state_test_helper_->ConfigureService(ss.str());
  }

  // |network_state| is a shill network state, e.g. "shill::kStateIdle".
  void SetWifiNetworkState(std::string network_state) {
    network_state_test_helper_->SetServiceProperty(wifi_network_service_path_,
                                                   shill::kStateProperty,
                                                   base::Value(network_state));
    task_environment_.RunUntilIdle();
  }

  // Used in tests, after |device_activity_client_| is generated.
  // Triggers the repeating timer in the client code.
  void FireTimer() {
    base::MockRepeatingTimer* mock_timer =
        static_cast<base::MockRepeatingTimer*>(
            device_activity_client_->GetReportTimer());
    if (mock_timer->IsRunning())
      mock_timer->Fire();
  }

  base::test::TaskEnvironment task_environment_;

  // The underlying |psm_test_data_| object will outlive this testing class.
  PsmTestData* psm_test_data_ = nullptr;

  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;
  TestingPrefServiceSimple local_state_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DeviceActivityClient> device_activity_client_;
  std::string wifi_network_service_path_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DeviceActivityClientTest, DefaultStatesAreInitializedProperly) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
  EXPECT_EQ(
      local_state_.GetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp),
      base::Time::UnixEpoch());
  EXPECT_TRUE(device_activity_client_->GetReportTimer()->IsRunning());
}

TEST_F(DeviceActivityClientTest, NetworkRequestsUseFakeApiKey) {
  // When network comes online, the device performs an Import network request.
  SetWifiNetworkState(shill::kStateOnline);

  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  task_environment_.RunUntilIdle();

  std::string api_key_header_value;
  request->request.headers.GetHeader("X-Goog-Api-Key", &api_key_header_value);

  EXPECT_EQ(api_key_header_value, kFakeFresnelApiKey);
}

// Fire timer to run |TransitionOutOfIdle|. Network is currently disconnected
// so the client is expected to go back to |kIdle| state.
TEST_F(DeviceActivityClientTest,
       FireTimerWithoutNetworkKeepsClientinIdleState) {
  SetWifiNetworkState(shill::kStateOffline);
  FireTimer();

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, PerformSuccessfulCheckIn) {
  // Device active reporting starts checking in on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingIn);

  base::Time prev_time =
      local_state_.GetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
      net::HTTP_OK);
  task_environment_.RunUntilIdle();

  base::Time new_time =
      local_state_.GetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp);

  // After a PSM identifier is checked in, the |local_state_|
  // |kDeviceActiveLastKnownDailyPingTimestamp| should be updated.
  EXPECT_LT(prev_time, new_time);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, NetworkReconnectsAfterSuccessfulCheckIn) {
  // Device active reporting starts checking in on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // Return well formed Import response body.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
      net::HTTP_OK);
  task_environment_.RunUntilIdle();

  // Reconnecting network connection triggers |TransitionOutOfIdle|.
  SetWifiNetworkState(shill::kStateOffline);
  SetWifiNetworkState(shill::kStateOnline);

  // Check that no additional network requests are pending since the PSM id
  // has already been imported.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(DeviceActivityClientTest, CheckInAfterNextUtcMidnight) {
  // Device active reporting starts membership check on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // Return well formed Import response body.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
      net::HTTP_OK);
  task_environment_.RunUntilIdle();

  // Return back to |kIdle| state after a successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  task_environment_.FastForwardBy(TimeUntilNextUTCMidnight());
  task_environment_.RunUntilIdle();

  FireTimer();

  // Check that additional network requests are pending since the PSM id
  // has NOT been imported for the new UTC day.
  EXPECT_GT(test_url_loader_factory_.NumPending(), 0);

  // Verify state goes directly to |kCheckingIn| since local state is updated
  // with the last check in timestamp.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingIn);

  // Mock Successful |kCheckingIn|.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
      net::HTTP_OK);
  task_environment_.RunUntilIdle();

  // Return back to |kIdle| state after second successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, DoNotCheckInTwiceBeforeNextUtcDay) {
  // Device active reporting starts checking in on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // Return well formed Import response body.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
      net::HTTP_OK);
  task_environment_.RunUntilIdle();

  // Return back to |kIdle| state after the first successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  base::TimeDelta before_utc_meridian =
      TimeUntilNextUTCMidnight() - base::Minutes(1);
  task_environment_.FastForwardBy(before_utc_meridian);
  task_environment_.RunUntilIdle();

  // Trigger attempt to report device active.
  FireTimer();

  // Client should not send any network requests since device is still in same
  // UTC day.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Remains in |kIdle| state since the device is still in same UTC day.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

// Powerwashing a device resets the |local_state_|. This will result in the
// client re-importing a PSM ID, on the same day.
TEST_F(DeviceActivityClientTest, CheckInAgainOnLocalStateReset) {
  // Device active reporting starts membership check on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  base::Time prev_time =
      local_state_.GetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp);

  // Mock Successful |kCheckingIn|.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
      net::HTTP_OK);
  task_environment_.RunUntilIdle();

  base::Time new_time =
      local_state_.GetTime(prefs::kDeviceActiveLastKnownDailyPingTimestamp);

  // After a PSM identifier is checked in, the |local_state_|
  // |kDeviceActiveLastKnownDailyPingTimestamp| should be updated.
  EXPECT_LT(prev_time, new_time);

  // Simulate powerwashing device by resetting the |local_state_|.
  local_state_.RemoveUserPref(prefs::kDeviceActiveLastKnownDailyPingTimestamp);

  // Retrigger |TransitionOutOfIdle| codepath by either firing timer or
  // reconnecting network.
  FireTimer();

  // Verify that the |kCheckingIn| state is reached.
  // Indicator is used to verify that we are checking the PSM ID again after
  // powerwash/recovery scenario.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingIn);
}

TEST_F(DeviceActivityClientTest, InitialUmaHistogramStateCount) {
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActiveClient.StateCount",
      DeviceActivityClient::State::kCheckingMembershipOprf, 0);
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActiveClient.StateCount",
      DeviceActivityClient::State::kCheckingMembershipQuery, 0);
  histogram_tester_.ExpectBucketCount("Ash.DeviceActiveClient.StateCount",
                                      DeviceActivityClient::State::kCheckingIn,
                                      0);
}

TEST_F(DeviceActivityClientTest, UmaHistogramStateCountAfterFirstCheckIn) {
  // Device active reporting starts membership check on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // Mock successful |kCheckingIn| requests.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
      net::HTTP_OK);
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectBucketCount("Ash.DeviceActiveClient.StateCount",
                                      DeviceActivityClient::State::kCheckingIn,
                                      1);
}

}  // namespace device_activity
}  // namespace ash
