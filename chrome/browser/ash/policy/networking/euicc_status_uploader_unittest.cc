// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/networking/euicc_status_uploader.h"

#include "ash/constants/ash_pref_names.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/network/network_ui_data.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace policy {

namespace {

class FakeCloudPolicyClient : public testing::NiceMock<MockCloudPolicyClient> {
 public:
  void SetStatus(bool status) { status_ = status; }

  enterprise_management::UploadEuiccInfoRequest* GetLastRequest() {
    if (requests_.empty())
      return nullptr;
    return requests_.back().get();
  }

  int num_requests() const { return requests_.size(); }

 private:
  void UploadEuiccInfo(
      std::unique_ptr<enterprise_management::UploadEuiccInfoRequest> request,
      base::OnceCallback<void(bool)> callback) override {
    requests_.push_back(std::move(request));
    std::move(callback).Run(status_);
  }

  std::vector<std::unique_ptr<enterprise_management::UploadEuiccInfoRequest>>
      requests_;
  bool status_ = false;
};

bool RequestsAreEqual(
    const enterprise_management::UploadEuiccInfoRequest& lhs,
    const enterprise_management::UploadEuiccInfoRequest& rhs) {
  return lhs.euicc_count() == rhs.euicc_count() &&
         std::equal(std::begin(lhs.esim_profiles()),
                    std::end(lhs.esim_profiles()),
                    std::begin(rhs.esim_profiles()),
                    [](const auto& u, const auto& v) {
                      return std::tie(u.iccid(), u.smdp_address()) ==
                             std::tie(v.iccid(), v.smdp_address());
                    }) &&
         lhs.clear_profile_list() == rhs.clear_profile_list();
}

const char kDmToken[] = "token";
const char kFakeObjectPath[] = "object-path";
const char kFakeEid[] = "12";
const char kEuiccStatusUploadResultHistogram[] =
    "Network.Cellular.ESim.Policy.EuiccStatusUploadResult";

const char kEmptyEuiccStatus[] =
    R"(
{
  "esim_profiles":[],"euicc_count":0
})";
const char kEuiccStatusWithOneProfile[] =
    R"({
        "esim_profiles":
          [
            {"iccid":"iccid-1","smdp_address":"smdp-1"}
          ],
        "euicc_count":2
       })";
const char kEuiccStatusWithTwoProfiles[] =
    R"({
        "esim_profiles":
          [
            {"iccid":"iccid-1","smdp_address":"smdp-1"},
            {"iccid":"iccid-2","smdp_address":"smdp-2"}
          ],
        "euicc_count":3
       })";
const char kEuiccStatusAfterReset[] =
    R"(
{
  "esim_profiles":[],"euicc_count":2
})";

const char kDefaultProfilePath[] = "/profile/default";

const char kCellularDevicePath[] = "/service/cellular1";
const char kCellularDevicePath2[] = "/service/cellular2";

struct FakeESimProfile {
  std::string service_path;
  std::string guid;
  std::string iccid;
  std::string smdp_address;
  bool managed = true;
};
struct EuiccTestData {
  int euicc_count = 0;
  // multiple euicc ids.
  std::vector<FakeESimProfile> profiles;
};

const EuiccTestData kSetupOneEsimProfile = {
    2,
    {{kCellularDevicePath, "guid-1", "iccid-1", "smdp-1", true}}};
const EuiccTestData kSetupTwoEsimProfiles = {
    3,
    {
        {kCellularDevicePath, "guid-1", "iccid-1", "smdp-1", true},
        {kCellularDevicePath2, "guid-2", "iccid-2", "smdp-2", true},
    }};
const EuiccTestData kSetupAfterReset = {2, {}};

}  // namespace

class EuiccStatusUploaderTest : public testing::Test {
 public:
  EuiccStatusUploaderTest() {}

  void SetUp() override {
    helper_ = std::make_unique<ash::NetworkHandlerTestHelper>();

    EuiccStatusUploader::RegisterLocalStatePrefs(local_state_.registry());
    helper_->RegisterPrefs(nullptr, local_state_.registry());
    helper_->InitializePrefs(nullptr, &local_state_);
    SetPolicyClientIsRegistered(/*is_registered=*/true);
  }

  std::unique_ptr<EuiccStatusUploader> CreateStatusUploader(
      bool is_policy_fetched = true) {
    auto status_uploader = base::WrapUnique(new EuiccStatusUploader(
        &cloud_policy_client_, &local_state_,
        base::BindRepeating(&EuiccStatusUploaderTest::is_device_active,
                            base::Unretained(this))));
    if (is_policy_fetched) {
      SetPolicyFetched(status_uploader.get());
    }
    return status_uploader;
  }

  void SetPolicyClientIsRegistered(bool is_registered) {
    cloud_policy_client_.dm_token_ = is_registered ? kDmToken : std::string();
  }

  void SetPolicyFetched(EuiccStatusUploader* status_uploader) {
    status_uploader->OnPolicyFetched(&cloud_policy_client_);
  }

  void SetServerSuccessStatus(bool success) {
    cloud_policy_client_.SetStatus(success);
  }

  const base::Value* GetStoredPref() {
    return local_state_.Get(EuiccStatusUploader::kLastUploadedEuiccStatusPref);
  }

  std::string GetStoredPrefString() {
    const base::Value* last_uploaded_pref = GetStoredPref();
    std::string result;
    JSONStringValueSerializer sz(&result);
    sz.Serialize(*last_uploaded_pref);
    return result;
  }

  void UpdateUploader(EuiccStatusUploader* status_uploader) {
    (static_cast<chromeos::NetworkPolicyObserver*>(status_uploader))
        ->PoliciesApplied(/*userhash=*/std::string());
    status_uploader->FireRetryTimerIfExistsForTesting();
  }

  void SetupEuicc(int euicc_id = 0) {
    chromeos::HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(base::StringPrintf("%s%d", kFakeObjectPath, euicc_id)),
        base::StringPrintf("%s%d", kFakeEid, euicc_id), /*is_active=*/true,
        euicc_id);
  }

  void SetUpDeviceProfiles(const EuiccTestData& data, bool add_to_onc = true) {
    // Create |data.euicc_count| fake EUICCs.
    chromeos::HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    for (int euicc_id = 0; euicc_id < data.euicc_count; euicc_id++) {
      SetupEuicc(euicc_id);
    }

    ash::ShillServiceClient::TestInterface* shill_service_client =
        ash::ShillServiceClient::Get()->GetTestInterface();
    shill_service_client->ClearServices();

    base::Value onc_config(base::Value::Type::LIST);
    for (const auto& test_profile : data.profiles) {
      shill_service_client->AddService(
          test_profile.service_path, test_profile.guid, /*name=*/"cellular",
          shill::kTypeCellular, "ready", /*visible=*/true);
      shill_service_client->SetServiceProperty(test_profile.service_path,
                                               shill::kIccidProperty,
                                               base::Value(test_profile.iccid));
      shill_service_client->SetServiceProperty(
          test_profile.service_path, shill::kProfileProperty,
          base::Value(kDefaultProfilePath));
      shill_service_client->SetServiceProperty(
          test_profile.service_path, shill::kUIDataProperty,
          base::Value(chromeos::NetworkUIData::CreateFromONC(
                          ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY)
                          ->GetAsJson()));

      base::Value single_onc = chromeos::onc::ReadDictionaryFromJson(
          R"({
              "GUID": ")" +
          test_profile.guid + R"(",
              "Name": "Cellular network",
              "Type": "Cellular",
              "Cellular": {
                "SMDPAddress" : ")" +
          test_profile.smdp_address + R"(",
              },
      })");
      onc_config.Append(std::move(single_onc));
    }

    if (add_to_onc) {
      // Set ONC values.
      chromeos::NetworkHandler::Get()
          ->managed_network_configuration_handler()
          ->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                      std::string() /* no username hash */,
                      std::move(onc_config), base::DictionaryValue());
    }

    // Wait for Shill device and service change notifications to propagate.
    base::RunLoop().RunUntilIdle();
  }

  void ValidateUploadedStatus(const std::string& expected_status_str,
                              bool clear_profile_list) {
    base::Value expected_status = base::test::ParseJson(expected_status_str);
    EXPECT_EQ(expected_status, *GetStoredPref());
    EXPECT_TRUE(cloud_policy_client_.GetLastRequest());
    EXPECT_TRUE(
        RequestsAreEqual(*EuiccStatusUploader::ConstructRequestFromStatus(
                             expected_status, clear_profile_list),
                         *cloud_policy_client_.GetLastRequest()));
  }

  void SetLastUploadedValue(const std::string& last_value) {
    local_state_.Set(EuiccStatusUploader::kLastUploadedEuiccStatusPref,
                     base::test::ParseJson(last_value));
  }

  void ExecuteResetCommand(EuiccStatusUploader* status_uploader) {
    SetUpDeviceProfiles(kSetupAfterReset);

    // TODO(crbug.com/1269719): Make FakeHermesEuiccClient trigger OnEuiccReset
    // directly.
    static_cast<chromeos::HermesEuiccClient::Observer*>(status_uploader)
        ->OnEuiccReset(dbus::ObjectPath());
  }

  int GetRequestCount() { return cloud_policy_client_.num_requests(); }

  void CheckHistogram(int total_count, int success_count, int failed_count) {
    histogram_tester_.ExpectTotalCount(kEuiccStatusUploadResultHistogram,
                                       total_count);
    histogram_tester_.ExpectBucketCount(kEuiccStatusUploadResultHistogram, true,
                                        /*expected_count=*/success_count);
    histogram_tester_.ExpectBucketCount(kEuiccStatusUploadResultHistogram,
                                        false,
                                        /*expected_count=*/failed_count);
  }

  void SetIsDeviceActive(bool value) { is_device_active_ = value; }

 private:
  bool is_device_active() { return is_device_active_; }

  bool is_device_active_ = true;
  content::BrowserTaskEnvironment task_environment_;
  FakeCloudPolicyClient cloud_policy_client_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> helper_;
  base::HistogramTester histogram_tester_;
};

TEST_F(EuiccStatusUploaderTest, EmptySetup) {
  auto status_uploader = CreateStatusUploader();
  EXPECT_EQ(GetRequestCount(), 0);
  // No value is uploaded yet.
  EXPECT_EQ("{}", GetStoredPrefString());

  // Make server accept requests.
  SetServerSuccessStatus(true);
  UpdateUploader(status_uploader.get());
  // Verify that no status is uploaded if there is no EUICC.
  EXPECT_EQ(GetRequestCount(), 0);
  CheckHistogram(/*total_count=*/0, /*success_count=*/0, /*failed_count=*/0);
}

TEST_F(EuiccStatusUploaderTest, InactiveDevice) {
  SetIsDeviceActive(false);
  auto status_uploader = CreateStatusUploader();
  EXPECT_EQ(GetRequestCount(), 0);
  // No value is uploaded yet.
  EXPECT_EQ("{}", GetStoredPrefString());

  // Make server accept requests.
  SetServerSuccessStatus(true);
  UpdateUploader(status_uploader.get());
  // Verify that no status is uploaded if the device is inactive.
  EXPECT_EQ(GetRequestCount(), 0);
  CheckHistogram(/*total_count=*/0, /*success_count=*/0, /*failed_count=*/0);
}

TEST_F(EuiccStatusUploaderTest, ClientNotRegistered) {
  SetupEuicc();
  base::RunLoop().RunUntilIdle();
  SetPolicyClientIsRegistered(/*is_registered=*/false);

  auto status_uploader = CreateStatusUploader();
  EXPECT_EQ(GetRequestCount(), 0);
  // No value is uploaded yet.
  EXPECT_EQ("{}", GetStoredPrefString());

  UpdateUploader(status_uploader.get());
  // Verify that no requests are made if client is not registered.
  EXPECT_EQ(GetRequestCount(), 0);
  EXPECT_EQ("{}", GetStoredPrefString());
  CheckHistogram(/*total_count=*/0, /*success_count=*/0, /*failed_count=*/0);
}

TEST_F(EuiccStatusUploaderTest, ServerError) {
  SetupEuicc();
  base::RunLoop().RunUntilIdle();
  auto status_uploader = CreateStatusUploader();
  UpdateUploader(status_uploader.get());
  EXPECT_EQ(GetRequestCount(), 2);
  // Nothing is stored when requests fail.
  EXPECT_EQ("{}", GetStoredPrefString());
  CheckHistogram(/*total_count=*/2, /*success_count=*/0, /*failed_count=*/2);
}

TEST_F(EuiccStatusUploaderTest, WaitForPolicyFetch) {
  SetUpDeviceProfiles(kSetupOneEsimProfile);

  auto status_uploader = CreateStatusUploader(/*is_policy_fetched=*/false);
  EXPECT_EQ(GetRequestCount(), 0);
  // No value is uploaded yet.
  EXPECT_EQ("{}", GetStoredPrefString());

  // Verify that no requests are made when policy has not been fetched.
  SetServerSuccessStatus(true);
  UpdateUploader(status_uploader.get());
  EXPECT_EQ(GetRequestCount(), 0);

  // Verify that status is uploaded correctly when policy is fetched.
  SetPolicyFetched(status_uploader.get());
  ValidateUploadedStatus(kEuiccStatusWithOneProfile,
                         /*clear_profile_list=*/false);
  CheckHistogram(/*total_count=*/1, /*success_count=*/1, /*failed_count=*/0);
}

TEST_F(EuiccStatusUploaderTest, Basic) {
  SetUpDeviceProfiles(kSetupOneEsimProfile);

  auto status_uploader = CreateStatusUploader();
  // Initial upload request.
  EXPECT_EQ(GetRequestCount(), 1);
  // No value is uploaded yet.
  EXPECT_EQ("{}", GetStoredPrefString());
  CheckHistogram(/*total_count=*/1, /*success_count=*/0, /*failed_count=*/1);

  // Make server accept requests.
  SetServerSuccessStatus(true);
  UpdateUploader(status_uploader.get());
  EXPECT_EQ(GetRequestCount(), 2);
  // Verify that last uploaded configuration is stored.
  ValidateUploadedStatus(kEuiccStatusWithOneProfile,
                         /*clear_profile_list=*/false);
  CheckHistogram(/*total_count=*/2, /*success_count=*/1, /*failed_count=*/1);
}

TEST_F(EuiccStatusUploaderTest, MultipleProfiles) {
  SetUpDeviceProfiles(kSetupTwoEsimProfiles);

  auto status_uploader = CreateStatusUploader();
  // Initial upload request.
  EXPECT_EQ(GetRequestCount(), 1);
  // No value is uploaded yet.
  EXPECT_EQ("{}", GetStoredPrefString());
  CheckHistogram(/*total_count=*/1, /*success_count=*/0, /*failed_count=*/1);

  // Make server accept requests.
  SetServerSuccessStatus(true);
  UpdateUploader(status_uploader.get());
  EXPECT_EQ(GetRequestCount(), 2);

  // Verify that last uploaded configuration is stored.
  ValidateUploadedStatus(kEuiccStatusWithTwoProfiles,
                         /*clear_profile_list=*/false);
  CheckHistogram(/*total_count=*/2, /*success_count=*/1, /*failed_count=*/1);
}

TEST_F(EuiccStatusUploaderTest, SameValueAsBefore) {
  // Make server accept requests.
  SetServerSuccessStatus(true);
  // Mark the current state as already uploaded.
  SetUpDeviceProfiles(kSetupOneEsimProfile);
  SetLastUploadedValue(kEuiccStatusWithOneProfile);

  auto status_uploader = CreateStatusUploader();
  // No value is uploaded since it has been previously sent.
  EXPECT_EQ(GetRequestCount(), 0);
  CheckHistogram(/*total_count=*/0, /*success_count=*/0, /*failed_count=*/0);
}

TEST_F(EuiccStatusUploaderTest, NewValue) {
  // Make server accept requests.
  SetServerSuccessStatus(true);
  // Set up a value different from one that was previously uploaded.
  SetUpDeviceProfiles(kSetupOneEsimProfile);
  SetLastUploadedValue(kEmptyEuiccStatus);

  auto status_uploader = CreateStatusUploader();
  // Verify that last uploaded configuration is stored.
  ValidateUploadedStatus(kEuiccStatusWithOneProfile,
                         /*clear_profile_list=*/false);
  CheckHistogram(/*total_count=*/1, /*success_count=*/1, /*failed_count=*/0);
}

TEST_F(EuiccStatusUploaderTest, ResetRequest) {
  // Make server accept requests.
  SetServerSuccessStatus(true);
  // Set up a value different from one that was previously uploaded.
  SetUpDeviceProfiles(kSetupOneEsimProfile);
  SetLastUploadedValue(kEmptyEuiccStatus);

  auto status_uploader = CreateStatusUploader();
  // Verify that last uploaded configuration is stored.
  ValidateUploadedStatus(kEuiccStatusWithOneProfile,
                         /*clear_profile_list=*/false);

  // Reset remote command was received and executed.
  ExecuteResetCommand(status_uploader.get());
  // Request has been sent.
  EXPECT_EQ(GetRequestCount(), 3);

  ValidateUploadedStatus(kEuiccStatusAfterReset,
                         /*clear_profile_list=*/true);

  // Send the reset command again.
  ExecuteResetCommand(status_uploader.get());
  // Request will be force-sent again because we've received a reset command..
  EXPECT_EQ(GetRequestCount(), 4);

  ValidateUploadedStatus(kEuiccStatusAfterReset,
                         /*clear_profile_list=*/true);
}

TEST_F(EuiccStatusUploaderTest, UnexpectedNetworkHandlerShutdown) {
  SetUpDeviceProfiles(kSetupOneEsimProfile);
  // NetworkHandler has not been initialized.
  auto status_uploader = CreateStatusUploader();

  // Initial Request
  EXPECT_EQ(GetRequestCount(), 1);

  // Requests made normally.
  UpdateUploader(status_uploader.get());
  EXPECT_EQ(GetRequestCount(), 2);

  // NetworkHandler::Shutdown() has already been called before
  // EuiccStatusUploader is deleted
  chromeos::NetworkHandler::Shutdown();

  // No requests made as NetworkHandler is not available.
  UpdateUploader(status_uploader.get());
  EXPECT_EQ(GetRequestCount(), 2);

  // Need to reinitialize before exiting test.
  chromeos::NetworkHandler::Initialize();
}

// A regression test for b/220933904 to verify that there should be no crash
// when the cellular policy is not found from the device ONC but the network
// state still exists.
TEST_F(EuiccStatusUploaderTest, ShouldNotCrashIfNoPolicyFound) {
  SetUpDeviceProfiles(kSetupOneEsimProfile, /*add_to_onc=*/false);

  auto status_uploader = CreateStatusUploader();
  // Initial upload request.
  EXPECT_EQ(GetRequestCount(), 1);
  // No value is uploaded yet.
  EXPECT_EQ("{}", GetStoredPrefString());
  CheckHistogram(/*total_count=*/1, /*success_count=*/0, /*failed_count=*/1);

  // Make server accept requests.
  SetServerSuccessStatus(true);
  UpdateUploader(status_uploader.get());
  EXPECT_EQ(GetRequestCount(), 2);
  CheckHistogram(/*total_count=*/2, /*success_count=*/1, /*failed_count=*/1);
}

}  // namespace policy
