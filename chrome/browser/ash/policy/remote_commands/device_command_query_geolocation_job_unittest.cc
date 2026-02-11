// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/policy/remote_commands/device_command_query_geolocation_job.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/remote_commands/crd/fake_start_crd_session_job_delegate.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_test_device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/live_location_provider.h"
#include "chromeos/ash/components/geolocation/location_fetcher.h"
#include "chromeos/ash/components/geolocation/location_provider.h"
#include "chromeos/ash/components/geolocation/system_location_provider.h"
#include "chromeos/ash/components/geolocation/test_utils.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {
namespace em = enterprise_management;
namespace test_utils = ash::geolocation::test_utils;

namespace {

const RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
// Helper to create the command proto.
em::RemoteCommand GenerateCommandProto(base::TimeDelta age_of_command) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_QUERY_GEOLOCATION);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  return command_proto;
}

GURL GetGeolocationUrl() {
  return GURL(ash::geolocation::test_utils::kTestGeolocationProviderUrl);
}

}  // namespace

class TestDeviceCloudPolicyManagerAsh : public DeviceCloudPolicyManagerAsh {
 public:
  TestDeviceCloudPolicyManagerAsh(
      std::unique_ptr<DeviceCloudPolicyStoreAsh> store,
      std::unique_ptr<CloudExternalDataManager> external_data_manager)
      : DeviceCloudPolicyManagerAsh(
            std::move(store),
            std::move(external_data_manager),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            &state_keys_broker_,
            crd_delegate_) {}
  ~TestDeviceCloudPolicyManagerAsh() override = default;

 private:
  ash::FakeSessionManagerClient fake_session_manager_client_;
  ServerBackedStateKeysBroker state_keys_broker_{&fake_session_manager_client_};
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  policy::FakeStartCrdSessionJobDelegate crd_delegate_;
};

class DeviceCommandQueryGeolocationJobTest : public testing::Test {
 public:
  DeviceCommandQueryGeolocationJobTest() {
    // Always enable the separate API key feature for these tests.
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kCrosSeparateGeoApiKey);
  }
  DeviceCommandQueryGeolocationJobTest(
      const DeviceCommandQueryGeolocationJobTest&) = delete;
  DeviceCommandQueryGeolocationJobTest& operator=(
      const DeviceCommandQueryGeolocationJobTest&) = delete;
  ~DeviceCommandQueryGeolocationJobTest() override = default;

  void SetUp() override {
    ash::DBusThreadManager::Initialize();
    ash::DeviceSettingsService::Initialize();
    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();

    std::unique_ptr<DeviceCloudPolicyStoreAsh> store =
        std::make_unique<DeviceCloudPolicyStoreAsh>(
            ash::DeviceSettingsService::Get(), test_install_attributes_.Get(),
            base::SingleThreadTaskRunner::GetCurrentDefault());

    auto external_data_manager =
        std::make_unique<MockCloudExternalDataManager>();

    test_manager_ = std::make_unique<TestDeviceCloudPolicyManagerAsh>(
        std::move(store), std::move(external_data_manager));
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_manager_->Initialize(pref_service_.get());

    // Initialize SystemLocationProvider for the test.
    ash::SystemLocationProvider::Initialize(
        std::make_unique<
            ash::LiveLocationProvider>(std::make_unique<ash::LocationFetcher>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            GetGeolocationUrl(),
            /*permission_context=*/nullptr)));
    ash::SystemLocationProvider* system_location_provider =
        ash::SystemLocationProvider::GetInstance();
    ASSERT_TRUE(system_location_provider);
    ash::LocationFetcher* fetcher =
        system_location_provider->GetLocationProviderForTesting()
            ->GetLocationFetcherForTesting();
    ASSERT_TRUE(fetcher);
    fetcher->SetSharedUrlLoaderFactoryForTesting(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    // Set default policy to allow the command.
    SetDevicePolicy(em::DeviceState::DEVICE_MODE_DISABLED,
                    /*location_tracking_enabled=*/true);
  }

  void TearDown() override {
    ash::SystemLocationProvider* system_location_provider =
        ash::SystemLocationProvider::GetInstance();
    if (system_location_provider) {
      ash::LocationFetcher* fetcher =
          system_location_provider->GetLocationProviderForTesting()
              ->GetLocationFetcherForTesting();
      if (fetcher) {
        fetcher->SetSharedUrlLoaderFactoryForTesting(nullptr);
      }
    }
    test_manager_->Shutdown();
    network_handler_test_helper_.reset();
    ash::SystemLocationProvider::DestroyForTesting();
    ash::DeviceSettingsService::Shutdown();
    ash::DBusThreadManager::Shutdown();
  }

 protected:
  void SetDevicePolicy(em::DeviceState::DeviceMode device_mode,
                       bool location_tracking_enabled) {
    auto policy_data = std::make_unique<em::PolicyData>();
    em::DeviceState* device_state = policy_data->mutable_device_state();
    device_state->set_device_mode(device_mode);
    device_state->mutable_disabled_state()->set_location_tracking_enabled(
        location_tracking_enabled);
    test_manager_->device_store()->set_policy_data_for_testing(
        std::move(policy_data));
  }

  std::unique_ptr<DeviceCommandQueryGeolocationJob> CreateJob(
      base::TimeTicks issued_time,
      const DeviceCloudPolicyManagerAsh* manager) {
    auto job = std::make_unique<DeviceCommandQueryGeolocationJob>(manager);
    auto command_proto =
        GenerateCommandProto(base::TimeTicks::Now() - issued_time);
    EXPECT_TRUE(
        job->Init(base::TimeTicks::Now(), command_proto, em::SignedData()));
    EXPECT_EQ(kUniqueID, job->unique_id());
    EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
    return job;
  }

  void AddMockResponse(const GURL& url, const std::string& response_body) {
    test_url_loader_factory_.AddResponse(url.spec(), response_body);
  }

  void AddMockErrorResponse(const GURL& url, net::HttpStatusCode http_status) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        base::StrCat({"HTTP/1.1 ", base::NumberToString(http_status), " ",
                      net::GetHttpReasonPhrase(http_status)}));
    head->headers->GetMimeType(&head->mime_type);
    test_url_loader_factory_.AddResponse(
        url, std::move(head), /*content=*/"",
        network::URLLoaderCompletionStatus(net::OK));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::ScopedStubInstallAttributes test_install_attributes_{
      ash::StubInstallAttributes::CreateCloudManaged("test_domain", "test_id")};
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  base::TimeTicks test_start_time_ = base::TimeTicks::Now();
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestDeviceCloudPolicyManagerAsh> test_manager_;
};

TEST_F(DeviceCommandQueryGeolocationJobTest, CommandFailsIfNotDisabled) {
  SetDevicePolicy(em::DeviceState::DEVICE_MODE_NORMAL,
                  /*location_tracking_enabled=*/true);
  auto job = CreateJob(test_start_time_, test_manager_.get());
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait());
  EXPECT_EQ(job->status(), RemoteCommandJob::Status::FAILED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  ASSERT_TRUE(payload);

  const std::optional<base::Value> parsed_payload =
      base::JSONReader::Read(*payload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_payload.has_value());
  ASSERT_TRUE(parsed_payload->is_dict());
  const base::DictValue& dict = parsed_payload->GetDict();

  EXPECT_EQ(dict.FindInt("result_code"),
            std::optional<int>(
                em::QueryGeolocationCommandResultCode::DEVICE_NOT_DISABLED));
}

TEST_F(DeviceCommandQueryGeolocationJobTest,
       CommandFailsIfLocationTrackingDisabled) {
  SetDevicePolicy(em::DeviceState::DEVICE_MODE_DISABLED,
                  /*location_tracking_enabled=*/false);
  auto job = CreateJob(test_start_time_, test_manager_.get());
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait());
  EXPECT_EQ(job->status(), RemoteCommandJob::Status::FAILED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  ASSERT_TRUE(payload);

  const std::optional<base::Value> parsed_payload =
      base::JSONReader::Read(*payload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_payload.has_value());
  ASSERT_TRUE(parsed_payload->is_dict());
  const base::DictValue& dict = parsed_payload->GetDict();

  EXPECT_EQ(
      dict.FindInt("result_code"),
      std::optional<int>(
          em::QueryGeolocationCommandResultCode::LOCATION_TRACKING_DISABLED));
}

TEST_F(DeviceCommandQueryGeolocationJobTest, CommandFailsForUnmanagedDevice) {
  auto job = CreateJob(test_start_time_, /*manager=*/nullptr);

  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait());

  EXPECT_EQ(job->status(), RemoteCommandJob::Status::FAILED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  ASSERT_TRUE(payload);

  const std::optional<base::Value> parsed_payload =
      base::JSONReader::Read(*payload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_payload.has_value());
  ASSERT_TRUE(parsed_payload->is_dict());
  const base::DictValue& dict = parsed_payload->GetDict();

  EXPECT_EQ(dict.FindInt("result_code"),
            std::optional<int>(
                em::QueryGeolocationCommandResultCode::DEVICE_NOT_MANAGED));
}

TEST_F(DeviceCommandQueryGeolocationJobTest, GetLocationSuccess) {
  const double latitude = 51.0;
  const double longitude = -0.1;
  const double accuracy = 1200.4;
  const GURL geolocation_url = GetGeolocationUrl();
  AddMockResponse(geolocation_url, test_utils::kSimpleResponseBody);
  auto job = CreateJob(test_start_time_, test_manager_.get());
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait());
  EXPECT_EQ(job->status(), RemoteCommandJob::Status::SUCCEEDED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  ASSERT_TRUE(payload);
  const std::optional<base::Value> parsed_payload =
      base::JSONReader::Read(*payload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_payload.has_value());
  ASSERT_TRUE(parsed_payload->is_dict());
  const base::DictValue& dict = parsed_payload->GetDict();
  EXPECT_EQ(dict.FindDouble("latitude"), latitude);
  EXPECT_EQ(dict.FindDouble("longitude"), longitude);
  EXPECT_EQ(dict.FindDouble("accuracy"), accuracy);
  const std::string* timestamp_str = dict.FindString("query_time_ms");
  ASSERT_TRUE(timestamp_str);
  int64_t timestamp_val;
  EXPECT_TRUE(base::StringToInt64(*timestamp_str, &timestamp_val));
}

TEST_F(DeviceCommandQueryGeolocationJobTest, GetLocationTimeout) {
  auto job = CreateJob(test_start_time_, test_manager_.get());
  LOG(INFO) << "Not adding mock response for URL to simulate timeout: "
            << GetGeolocationUrl().spec();
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  task_environment_.FastForwardBy(base::Seconds(61));
  ASSERT_TRUE(job_finished_future.Wait());
  EXPECT_EQ(job->status(), RemoteCommandJob::Status::FAILED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  ASSERT_TRUE(payload);

  const std::optional<base::Value> parsed_payload =
      base::JSONReader::Read(*payload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_payload.has_value());
  ASSERT_TRUE(parsed_payload->is_dict());
  const base::DictValue& dict = parsed_payload->GetDict();
  EXPECT_EQ(dict.FindInt("result_code"),
            std::optional<int>(em::QueryGeolocationCommandResultCode::TIMEOUT));
  EXPECT_FALSE(dict.contains("error_code"));
  EXPECT_FALSE(dict.contains("error_message"));
}

TEST_F(DeviceCommandQueryGeolocationJobTest, GetLocationInvalidResponse) {
  const GURL geolocation_url = GetGeolocationUrl();
  AddMockResponse(geolocation_url, "{ \"invalid\": \"response\" }");
  auto job = CreateJob(test_start_time_, test_manager_.get());
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait());
  EXPECT_EQ(job->status(), RemoteCommandJob::Status::FAILED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  ASSERT_TRUE(payload);

  const std::optional<base::Value> parsed_payload =
      base::JSONReader::Read(*payload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_payload.has_value());
  ASSERT_TRUE(parsed_payload->is_dict());
  const base::DictValue& dict = parsed_payload->GetDict();

  EXPECT_EQ(dict.FindInt("result_code"),
            std::optional<int>(em::QueryGeolocationCommandResultCode::TIMEOUT));
  EXPECT_FALSE(dict.contains("error_code"));
  EXPECT_FALSE(dict.contains("error_message"));
}

TEST_F(DeviceCommandQueryGeolocationJobTest, GetLocationServerError) {
  const GURL geolocation_url = GetGeolocationUrl();
  AddMockResponse(
      geolocation_url,
      "{\"error\":{\"code\":400, \"message\":\"Internal server error\"}}");
  auto job = CreateJob(test_start_time_, test_manager_.get());
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait());
  EXPECT_EQ(job->status(), RemoteCommandJob::Status::FAILED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  ASSERT_TRUE(payload);

  const std::optional<base::Value> parsed_payload =
      base::JSONReader::Read(*payload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_payload.has_value());
  ASSERT_TRUE(parsed_payload->is_dict());
  const base::DictValue& dict = parsed_payload->GetDict();

  EXPECT_EQ(dict.FindInt("result_code"),
            std::optional<int>(em::QueryGeolocationCommandResultCode::TIMEOUT));
  EXPECT_EQ(dict.FindInt("error_code"), std::optional<int>(400));
  EXPECT_EQ(*dict.FindString("error_message"), "Internal server error");
}

TEST_F(DeviceCommandQueryGeolocationJobTest, GetLocationTooManyRequests) {
  const GURL geolocation_url = GetGeolocationUrl();
  AddMockErrorResponse(geolocation_url, net::HTTP_TOO_MANY_REQUESTS);
  auto job = CreateJob(test_start_time_, test_manager_.get());
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait());
  EXPECT_EQ(job->status(), RemoteCommandJob::Status::FAILED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  ASSERT_TRUE(payload);

  const std::optional<base::Value> parsed_payload =
      base::JSONReader::Read(*payload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_payload.has_value());
  ASSERT_TRUE(parsed_payload->is_dict());
  const base::DictValue& dict = parsed_payload->GetDict();

  EXPECT_EQ(
      dict.FindInt("result_code"),
      std::optional<int>(em::QueryGeolocationCommandResultCode::SERVER_ERROR));
  EXPECT_THAT(*dict.FindString("error_message"),
              testing::HasSubstr("No response received"));
}

TEST_F(DeviceCommandQueryGeolocationJobTest,
       GetLocationIncorrectLocationReturned) {
  const GURL geolocation_url = GetGeolocationUrl();
  AddMockResponse(geolocation_url, "{}");
  auto job = CreateJob(test_start_time_, test_manager_.get());
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait());
  EXPECT_EQ(job->status(), RemoteCommandJob::Status::FAILED);
  std::unique_ptr<std::string> payload = job->GetResultPayload();
  ASSERT_TRUE(payload);

  const std::optional<base::Value> parsed_payload =
      base::JSONReader::Read(*payload, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_payload.has_value());
  ASSERT_TRUE(parsed_payload->is_dict());
  const base::DictValue& dict = parsed_payload->GetDict();

  // The result code is TIMEOUT because when response is empty, it's being
  // retried until the retry limit is reached, and then the job is failed with
  // TIMEOUT result code.
  EXPECT_EQ(dict.FindInt("result_code"),
            std::optional<int>(em::QueryGeolocationCommandResultCode::TIMEOUT));
}

}  // namespace policy
