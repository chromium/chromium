// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

using testing::Eq;
using testing::Property;
using testing::SizeIs;
using testing::StartsWith;
using testing::StrEq;

namespace reporting {

constexpr char kDmToken[] = "fake-dm-token";
constexpr char kClientId[] = "fake-client-id";
constexpr char kServerUrl[] = "https://example.com/reporting";

constexpr char kResponseKey[] = "response_key";
constexpr char kResponseValue[] = "response_value";

constexpr int kGenerationId = 1;
constexpr char kEncryptedRecord[] = "encrypted-record";

class FakeDelegate : public EncryptedReportingClient::Delegate {
 public:
  explicit FakeDelegate(
      policy::DeviceManagementService* device_management_service)
      : device_management_service_(device_management_service) {}

  FakeDelegate(const FakeDelegate&) = delete;
  FakeDelegate& operator=(const FakeDelegate&) = delete;

  ~FakeDelegate() override = default;

  policy::DeviceManagementService* device_management_service() const override {
    return device_management_service_;
  }

 private:
  const raw_ptr<policy::DeviceManagementService> device_management_service_;
};

class EncryptedReportingClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, "fake-serial-number");
#endif

    device_management_service_ =
        std::make_unique<policy::DeviceManagementService>(
            std::make_unique<policy::DeviceManagementServiceConfiguration>(
                "", "", kServerUrl));
    device_management_service_->ScheduleInitialization(0);
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());
    BuildPayload();

    cloud_policy_client_.SetDMToken(kDmToken);
    cloud_policy_client_.client_id_ = kClientId;
  }

  void TearDown() override {
    policy::EncryptedReportingJobConfiguration::ResetUploadsStateForTest();
  }

  void BuildPayload() {
    context_.SetByDottedPath("browser.userAgent", "agent-test-value");

    base::Value::Dict sequence_information;
    sequence_information.Set(
        SequenceInformationDictionaryBuilder::GetGenerationIdPath(),
        base::NumberToString(kGenerationId));
    sequence_information.Set(
        SequenceInformationDictionaryBuilder::GetSequencingIdPath(),
        base::NumberToString(sequence_id_));
    sequence_information.Set(
        SequenceInformationDictionaryBuilder::GetPriorityPath(),
        Priority::SLOW_BATCH);

    base::Value::Dict encrypted_wrapped_record;
    encrypted_wrapped_record.Set(
        EncryptedRecordDictionaryBuilder::GetEncryptedWrappedRecordPath(),
        kEncryptedRecord);
    encrypted_wrapped_record.Set(
        EncryptedRecordDictionaryBuilder::GetSequenceInformationKeyPath(),
        std::move(sequence_information));

    base::Value::List record_list;
    record_list.Append(std::move(encrypted_wrapped_record));
    merging_payload_.Set(
        UploadEncryptedReportingRequestBuilder::GetEncryptedRecordListPath(),
        std::move(record_list));
  }

  void DecrementSequenceId() { --sequence_id_; }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::Value::Dict context_;
  base::Value::Dict merging_payload_;
  int sequence_id_ = 10;

  std::unique_ptr<policy::DeviceManagementService> device_management_service_;
  network::TestURLLoaderFactory url_loader_factory_;
  policy::MockCloudPolicyClient cloud_policy_client_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
};

TEST_F(EncryptedReportingClientTest, Default) {
  EncryptedReportingClient encrypted_reporting_client(
      std::make_unique<FakeDelegate>(device_management_service_.get()));

  {
    test::TestEvent<StatusOr<base::Value::Dict>> response_event;
    encrypted_reporting_client.UploadReport(
        std::move(merging_payload_), std::move(context_), &cloud_policy_client_,
        response_event.cb());
    base::RunLoop().RunUntilIdle();

    ASSERT_THAT(*url_loader_factory_.pending_requests(), SizeIs(1));

    // Verify request header contains dm token
    EXPECT_TRUE(base::Contains(
        (*url_loader_factory_.pending_requests())[0].request.headers.ToString(),
        kDmToken));

    const std::string& pending_request_url =
        (*url_loader_factory_.pending_requests())[0].request.url.spec();

    EXPECT_THAT(pending_request_url, StartsWith(kServerUrl));

    // Verify request contains dm token
    EXPECT_TRUE(base::Contains(
        (*url_loader_factory_.pending_requests())[0].request.headers.ToString(),
        kDmToken));

    url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request_url,
        base::StringPrintf(R"({"%s" : "%s"})", kResponseKey, kResponseValue));

    const auto actual_response = response_event.result();
    ASSERT_OK(actual_response);
    ASSERT_THAT(actual_response.ValueOrDie(), SizeIs(1));
    ASSERT_TRUE(actual_response.ValueOrDie().FindString(kResponseKey));
    EXPECT_THAT(*(actual_response.ValueOrDie().FindString(kResponseKey)),
                StrEq(kResponseValue));
  }

  // Avoid rate limiting by time, but still reject the upload because of lower
  // sequence id.
  task_environment_.FastForwardBy(base::Minutes(1));
  DecrementSequenceId();

  {
    BuildPayload();

    test::TestEvent<StatusOr<base::Value::Dict>> response_event;
    encrypted_reporting_client.UploadReport(
        std::move(merging_payload_), std::move(context_), &cloud_policy_client_,
        response_event.cb());

    // Sequence ID decreased, upload is rejected.
    const auto actual_response = response_event.result();
    EXPECT_THAT(actual_response,
                Property(&StatusOr<base::Value::Dict>::status,
                         AllOf(Property(&Status::code, Eq(error::OUT_OF_RANGE)),
                               Property(&Status::error_message,
                                        StrEq("Too many upload requests")))));
  }
}

TEST_F(EncryptedReportingClientTest, ServiceUnavailable) {
  EncryptedReportingClient encrypted_reporting_client(
      std::make_unique<FakeDelegate>(nullptr));

  test::TestEvent<StatusOr<base::Value::Dict>> response_event;
  encrypted_reporting_client.UploadReport(
      std::move(merging_payload_), std::move(context_), &cloud_policy_client_,
      response_event.cb());
  const auto actual_response = response_event.result();
  EXPECT_THAT(
      actual_response,
      Property(
          &StatusOr<base::Value::Dict>::status,
          AllOf(
              Property(&Status::code, Eq(error::NOT_FOUND)),
              Property(
                  &Status::error_message,
                  StrEq(
                      "Device management service required, but not found")))));
}

TEST_F(EncryptedReportingClientTest, ServiceRejectedByRateLimiting) {
  EncryptedReportingClient encrypted_reporting_client(
      std::make_unique<FakeDelegate>(device_management_service_.get()));

  {
    test::TestEvent<StatusOr<base::Value::Dict>> response_event;

    encrypted_reporting_client.UploadReport(
        merging_payload_.Clone(), context_.Clone(), &cloud_policy_client_,
        response_event.cb());
    base::RunLoop().RunUntilIdle();

    ASSERT_THAT(*url_loader_factory_.pending_requests(), SizeIs(1));

    // Verify request header contains dm token
    EXPECT_TRUE(base::Contains(
        (*url_loader_factory_.pending_requests())[0].request.headers.ToString(),
        kDmToken));

    const std::string& pending_request_url =
        (*url_loader_factory_.pending_requests())[0].request.url.spec();

    EXPECT_THAT(pending_request_url, StartsWith(kServerUrl));

    // Verify request contains dm token
    EXPECT_TRUE(base::Contains(
        (*url_loader_factory_.pending_requests())[0].request.headers.ToString(),
        kDmToken));

    url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request_url,
        base::StringPrintf(R"({"%s" : "%s"})", kResponseKey, kResponseValue));

    const auto actual_response = response_event.result();
    ASSERT_OK(actual_response);
    ASSERT_THAT(actual_response.ValueOrDie(), SizeIs(1));
    ASSERT_TRUE(actual_response.ValueOrDie().FindString(kResponseKey));
    EXPECT_THAT(*(actual_response.ValueOrDie().FindString(kResponseKey)),
                StrEq(kResponseValue));
  }

  // Repeat the same upload, get it rejected by rate limiter.
  task_environment_.FastForwardBy(base::Seconds(1));
  {
    test::TestEvent<StatusOr<base::Value::Dict>> response_event;
    encrypted_reporting_client.UploadReport(
        std::move(merging_payload_), std::move(context_), &cloud_policy_client_,
        response_event.cb());
    const auto actual_response = response_event.result();
    EXPECT_THAT(actual_response,
                Property(&StatusOr<base::Value::Dict>::status,
                         AllOf(Property(&Status::code, Eq(error::OUT_OF_RANGE)),
                               Property(&Status::error_message,
                                        StrEq("Too many upload requests")))));
  }
}

// Verify that when the cloud policy client isn't provided, device info is not
// added to the request headers.
TEST_F(EncryptedReportingClientTest, UploadSucceedsWithoutDeviceInfo) {
  // Set cloud policy client to be nullptr to indicate that device info is
  // not available, i.e. the device dm token should NOT exists in
  // the request headers.
  EncryptedReportingClient encrypted_reporting_client(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client.UploadReport(std::move(merging_payload_),
                                          std::move(context_), nullptr,
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(*url_loader_factory_.pending_requests(), SizeIs(1));

  // Verify request does NOT contain dm token
  EXPECT_FALSE(base::Contains(
      (*url_loader_factory_.pending_requests())[0].request.headers.ToString(),
      kDmToken));
}

}  // namespace reporting
