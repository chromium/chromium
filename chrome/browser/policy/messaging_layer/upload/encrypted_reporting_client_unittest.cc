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
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

using testing::ContainerEq;
using testing::ElementsAre;
using testing::Eq;
using testing::Ge;
using testing::IsEmpty;
using testing::Lt;
using testing::Property;
using testing::SizeIs;
using testing::StartsWith;
using testing::StrEq;

namespace reporting {

namespace {

constexpr char kDmToken[] = "fake-dm-token";
constexpr char kClientId[] = "fake-client-id";
constexpr char kServerUrl[] = "https://example.com/reporting";

constexpr int kGenerationId = 1;
constexpr int kFirstSequenceId = 1;
constexpr char kEncryptedRecord[] = "encrypted-record";

size_t RecordsSize(const std::vector<EncryptedRecord>& records) {
  size_t size = 0;
  for (const auto& record : records) {
    size += record.ByteSizeLong();
  }
  return size;
}

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
}  // namespace

class EncryptedReportingClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  "fake-serial-number");
#endif

    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4uL * 1024uL * 1024uL);

    device_management_service_ =
        std::make_unique<policy::DeviceManagementService>(
            std::make_unique<policy::DeviceManagementServiceConfiguration>(
                /*dm_server_url=*/"", /*realtime_reporting_server_url=*/"",
                /*encrypted_reporting_server_url=*/kServerUrl));
    device_management_service_->ScheduleInitialization(0);
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());

    context_.SetByDottedPath("browser.userAgent", "agent-test-value");
  }

  void TearDown() override {
    EncryptedReportingClient::ResetUploadsStateForTest();

    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  void AddRecordToPayload(Priority priority = Priority::SLOW_BATCH) {
    payload_records_.emplace_back();

    EncryptedRecord& encrypted_record = payload_records_.back();
    encrypted_record.set_encrypted_wrapped_record(kEncryptedRecord);

    SequenceInformation* const sequence_information =
        encrypted_record.mutable_sequence_information();
    sequence_information->set_generation_id(kGenerationId);
    sequence_information->set_sequencing_id(sequence_id_++);
    sequence_information->set_priority(priority);
  }

  void DecrementSequenceId(int64_t by = 0L) { sequence_id_ -= (by + 1L); }

  base::Value::Dict GetRequestBody(size_t index, bool expect_dm_token = true) {
    CHECK_LT(index, url_loader_factory_.pending_requests()->size());
    const network::ResourceRequest& request =
        (*url_loader_factory_.pending_requests())[index].request;
    if (expect_dm_token) {
      EXPECT_TRUE(base::Contains(request.headers.ToString(), kDmToken));
    } else {
      EXPECT_FALSE(base::Contains(request.headers.ToString(), kDmToken));
    }
    CHECK(request.request_body);
    CHECK(request.request_body->elements());

    std::optional<base::Value> body =
        base::JSONReader::Read(request.request_body->elements()
                                   ->at(0)
                                   .As<network::DataElementBytes>()
                                   .AsStringPiece());
    CHECK(body);
    CHECK(body->is_dict());
    return body->GetDict().Clone();
  }

  void SimulateCustomResponseForRequest(size_t index,
                                        StatusOr<base::Value::Dict> response) {
    ASSERT_THAT(index, Lt(url_loader_factory_.pending_requests()->size()));
    const std::string& pending_request_url =
        (*url_loader_factory_.pending_requests())[index].request.url.spec();
    EXPECT_THAT(pending_request_url, StartsWith(kServerUrl));

    std::string response_string = "";
    if (response.has_value()) {
      base::JSONWriter::Write(response.value(), &response_string);
    }
    url_loader_factory_.SimulateResponseForPendingRequest(pending_request_url,
                                                          response_string);
  }

  UploadResponseParser GetAndValidateResponse(
      test::TestEvent<StatusOr<UploadResponseParser>>& response_event,
      std::optional<int64_t> expected_seq_id = std::nullopt) const {
    auto actual_response = response_event.result();
    CHECK(actual_response.has_value()) << actual_response.error();
    if (actual_response.value()
            .last_successfully_uploaded_record_sequence_info()
            .has_value()) {
      EXPECT_THAT(actual_response.value()
                      .last_successfully_uploaded_record_sequence_info()
                      .value()
                      .sequencing_id(),
                  Eq(expected_seq_id.has_value() ? expected_seq_id.value()
                                                 : payload_records_.rbegin()
                                                       ->sequence_information()
                                                       .sequencing_id()));
    } else {
      // No confirmation - at least envcryption key is to be expected.
      EXPECT_TRUE(actual_response.value().encryption_settings().has_value());
    }
    return std::move(actual_response.value());
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Set up this device as a managed device.
  policy::ScopedManagementServiceOverrideForTesting scoped_management_service_ =
      policy::ScopedManagementServiceOverrideForTesting(
          policy::ManagementServiceFactory::GetForPlatform(),
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  scoped_refptr<ResourceManager> memory_resource_;
  base::Value::Dict context_;
  bool need_encryption_key_ = false;
  int config_file_version_ = 0;
  std::vector<EncryptedRecord> payload_records_;
  int sequence_id_ = kFirstSequenceId;

  std::unique_ptr<policy::DeviceManagementService> device_management_service_;
  network::TestURLLoaderFactory url_loader_factory_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
};

TEST_F(EncryptedReportingClientTest, RegularUploads) {
  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  // Send record kFirstSequenceId for upload.
  {
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(1L));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response confirming
    // kFirstSequenceId.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(response_event));
  }

  // Send record #11 for upload.
  // Avoid rate limiting by time.
  task_environment_.FastForwardBy(base::Minutes(1));
  {
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(2L));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response confirming #11.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(response_event));
  }
}

TEST_F(EncryptedReportingClientTest, TimedOutUploadWithSameRecords) {
  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  // Send record kFirstSequenceId for upload.
  {
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(1L));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    // Do not send the response, let the job time out.
    task_environment_.FastForwardBy(
        EncryptedReportingClient::kReportingUploadDeadline);

    const auto& actual_response = response_event.result();
    EXPECT_FALSE(actual_response.has_value());
  }

  // Send record kFirstSequenceId for upload again.
  // Avoid rate limiting by time.
  task_environment_.FastForwardBy(base::Minutes(1));
  {
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(1L));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response.
    // Skip the first request (from the cancelled jobs), respond to the last!
    auto request_body = GetRequestBody(/*index=*/1);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/1, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(response_event));
  }
}

TEST_F(EncryptedReportingClientTest, TimedOutUploadWithAddedRecord) {
  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  // Send record kFirstSequenceId for upload.
  {
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(1L));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    // Do not respond, let the job time out.
    task_environment_.FastForwardBy(
        EncryptedReportingClient::kReportingUploadDeadline);

    const auto& actual_response = response_event.result();
    EXPECT_FALSE(actual_response.has_value());
  }

  // Send record #11 for upload
  // Avoid rate limiting by time.
  task_environment_.FastForwardBy(base::Minutes(1));
  {
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(1L, 2L));

    task_environment_.RunUntilIdle();

    // Skip the first request (from the cancelled jobs), respond to the last!
    auto request_body = GetRequestBody(/*index=*/1);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/1, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(response_event));
  }
}

TEST_F(EncryptedReportingClientTest, KeyRequestAlone) {
  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  // Send key request with no records.
  {
    ScopedReservation scoped_reservation(0uL, memory_resource_);
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        /*need_encryption_key=*/true, config_file_version_,
        std::vector<EncryptedRecord>(), std::move(scoped_reservation),
        enqueued_event.cb(), response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), IsEmpty());

    task_environment_.RunUntilIdle();

    // Request is created and delivered.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsEncryptionKeyRequestUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(response_event));
  }

  // Can repeat immediately - no throttling when there are no records.
  {
    ScopedReservation scoped_reservation(0uL, memory_resource_);
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        /*need_encryption_key=*/true, config_file_version_,
        std::vector<EncryptedRecord>(), std::move(scoped_reservation),
        enqueued_event.cb(), response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), IsEmpty());

    task_environment_.RunUntilIdle();

    // Request is created and delivered.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsEncryptionKeyRequestUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(response_event));
  }
}

TEST_F(EncryptedReportingClientTest, ForceConfirmAndRetract) {
  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  // Send record kFirstSequenceId for upload.
  {
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(1L));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response with force flag.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response =
        ResponseBuilder(std::move(request_body)).SetForceConfirm(true).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    const auto& actual_response = GetAndValidateResponse(response_event);
    EXPECT_TRUE(actual_response.force_confirm_flag());
  }

  // Send record #11 for upload.
  // Avoid rate limiting by time.
  task_environment_.FastForwardBy(base::Minutes(1));

  {
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(2L));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(response_event));
  }
}

TEST_F(EncryptedReportingClientTest, ServiceUnavailable) {
  auto encrypted_reporting_client =
      EncryptedReportingClient::Create(std::make_unique<FakeDelegate>(nullptr));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  // Send record kFirstSequenceId for processing. No connection to the service.
  AddRecordToPayload();
  ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                       memory_resource_);
  ASSERT_TRUE(scoped_reservation.reserved());
  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<StatusOr<UploadResponseParser>> response_event;
  encrypted_reporting_client->UploadReport(
      need_encryption_key_, config_file_version_, payload_records_,
      std::move(scoped_reservation), enqueued_event.cb(), response_event.cb());

  const auto& enqueued_result = enqueued_event.result();
  EXPECT_OK(enqueued_result);
  EXPECT_THAT(enqueued_result.value(), ElementsAre(1L));

  const auto& actual_response = response_event.result();
  EXPECT_THAT(
      actual_response,
      Property(
          &StatusOr<UploadResponseParser>::error,
          AllOf(
              Property(&Status::code, Eq(error::NOT_FOUND)),
              Property(
                  &Status::error_message,
                  StrEq(
                      "Device management service required, but not found")))));
}

TEST_F(EncryptedReportingClientTest, ServiceRejectedByRateLimiting) {
  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  // Send record kFirstSequenceId for upload.
  {
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(1L));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(response_event));
  }

  // Send record #11 for upload.
  // Get upload rejected by rate limiter, even though it has a new record.
  task_environment_.FastForwardBy(base::Seconds(1));
  {
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ElementsAre(2L));

    const auto& actual_response = response_event.result();
    EXPECT_THAT(actual_response,
                Property(&StatusOr<UploadResponseParser>::error,
                         AllOf(Property(&Status::code, Eq(error::OUT_OF_RANGE)),
                               Property(&Status::error_message,
                                        StrEq("Too many upload requests")))))
        << actual_response.error();
  }
}

// Verify that when the cloud policy client isn't provided, device info is not
// added to the request headers.
TEST_F(EncryptedReportingClientTest, UploadSucceedsWithoutDeviceInfo) {
  // Set cloud policy client to be nullptr to indicate that device info is
  // not available, i.e. the device dm token should NOT exists in
  // the request headers.
  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), "", "");

  // Send record kFirstSequenceId for upload.
  AddRecordToPayload();
  ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                       memory_resource_);
  ASSERT_TRUE(scoped_reservation.reserved());
  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<StatusOr<UploadResponseParser>> response_event;
  encrypted_reporting_client->UploadReport(
      need_encryption_key_, config_file_version_, payload_records_,
      std::move(scoped_reservation), enqueued_event.cb(), response_event.cb());

  const auto& enqueued_result = enqueued_event.result();
  EXPECT_OK(enqueued_result);
  EXPECT_THAT(enqueued_result.value(), ElementsAre(1L));

  task_environment_.RunUntilIdle();

  // Simulate server-side processing, generate response.
  auto request_body = GetRequestBody(/*index=*/0, /*expect_dm_token=*/false);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body)).Build();
  ASSERT_TRUE(response.has_value());
  SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

  base::IgnoreResult(GetAndValidateResponse(response_event));
}

TEST_F(EncryptedReportingClientTest, IdenticalUploadRetriesThrottled) {
  const size_t kTotalRetries = 10;

  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  std::list<int64_t> expected_cached_seq_ids;
  base::TimeDelta expected_delay_after = base::Seconds(10);
  for (size_t i = 0; i < kTotalRetries; ++i) {
    // Add one more record for upload.
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    expected_cached_seq_ids.push_back(
        payload_records_.rbegin()->sequence_information().sequencing_id());

    auto allowed_delay = encrypted_reporting_client->WhenIsAllowedToProceed(
        payload_records_.rbegin()->sequence_information().priority(),
        payload_records_.rbegin()->sequence_information().generation_id());
    if (i == 0) {
      // First upload allowed immediately.
      EXPECT_FALSE(allowed_delay.is_positive());
    } else {
      // Further uploads allowed with delay.
      EXPECT_THAT(allowed_delay, Ge(expected_delay_after));
      // Double the expectation for the next retry.
      expected_delay_after *= 2;
      // Move forward to allow.
      task_environment_.FastForwardBy(allowed_delay - base::Seconds(1));
      EXPECT_TRUE(
          encrypted_reporting_client
              ->WhenIsAllowedToProceed(
                  payload_records_.rbegin()->sequence_information().priority(),
                  payload_records_.rbegin()
                      ->sequence_information()
                      .generation_id())
              .is_positive());
      task_environment_.FastForwardBy(base::Seconds(1));
    }

    EXPECT_FALSE(
        encrypted_reporting_client
            ->WhenIsAllowedToProceed(
                payload_records_.rbegin()->sequence_information().priority(),
                payload_records_.rbegin()
                    ->sequence_information()
                    .generation_id())
            .is_positive());

    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());

    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());

    // Modify response to not confirm actual records.
    {
      auto* const last_successfully_uploaded_record =
          response.value().FindDict(json_keys::kLastSucceedUploadedRecord);
      ASSERT_TRUE(last_successfully_uploaded_record);
      last_successfully_uploaded_record->Set(
          json_keys::kSequencingId, base::NumberToString(kFirstSequenceId - 1));
    }

    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(
        response_event, /*expected_seq_id=*/kFirstSequenceId - 1));

    encrypted_reporting_client->AccountForAllowedJob(
        payload_records_.rbegin()->sequence_information().priority(),
        payload_records_.rbegin()->sequence_information().generation_id(),
        payload_records_.rbegin()->sequence_information().sequencing_id());
  }
}

TEST_F(EncryptedReportingClientTest, UploadsSequenceThrottled) {
  const size_t kTotalRetries = 10;

  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  std::list<int64_t> expected_cached_seq_ids;
  base::TimeDelta expected_delay_after = base::Seconds(10);
  for (size_t i = 0; i < kTotalRetries; ++i) {
    // Add one more record for upload.
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    expected_cached_seq_ids.push_back(
        payload_records_.rbegin()->sequencing_information().sequencing_id());

    auto allowed_delay = encrypted_reporting_client->WhenIsAllowedToProceed(
        payload_records_.rbegin()->sequence_information().priority(),
        payload_records_.rbegin()->sequence_information().generation_id());
    if (i == 0) {
      // First upload allowed immediately.
      EXPECT_FALSE(allowed_delay.is_positive());
    } else {
      // Further uploads allowed with delay.
      EXPECT_THAT(allowed_delay, Ge(expected_delay_after));
      // Double the expectation for the next retry.
      expected_delay_after *= 2;
      // Move forward to allow.
      task_environment_.FastForwardBy(allowed_delay - base::Seconds(1));
      EXPECT_TRUE(
          encrypted_reporting_client
              ->WhenIsAllowedToProceed(
                  payload_records_.rbegin()->sequence_information().priority(),
                  payload_records_.rbegin()
                      ->sequence_information()
                      .generation_id())
              .is_positive());
      task_environment_.FastForwardBy(base::Seconds(1));
    }

    EXPECT_FALSE(
        encrypted_reporting_client
            ->WhenIsAllowedToProceed(
                payload_records_.rbegin()->sequence_information().priority(),
                payload_records_.rbegin()
                    ->sequence_information()
                    .generation_id())
            .is_positive());

    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    if (i == 0) {
      // First time only one record expected in cache.
      EXPECT_THAT(enqueued_result.value(),
                  ElementsAre(payload_records_.rbegin()
                                  ->sequence_information()
                                  .sequencing_id()));
    } else {
      // After that 2 last records expected in cache.
      EXPECT_THAT(enqueued_result.value(),
                  ElementsAre(payload_records_.rbegin()
                                      ->sequence_information()
                                      .sequencing_id() -
                                  1L,
                              payload_records_.rbegin()
                                  ->sequence_information()
                                  .sequencing_id()));
    }

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());

    // Modify response to not confirm the last records.
    {
      auto* const last_successfully_uploaded_record =
          response.value().FindDict(json_keys::kLastSucceedUploadedRecord);
      ASSERT_TRUE(last_successfully_uploaded_record);
      last_successfully_uploaded_record->Set(
          json_keys::kSequencingId, base::NumberToString(sequence_id_ - 2));
    }

    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(
        response_event, /*expected_seq_id=*/sequence_id_ - 2));

    encrypted_reporting_client->AccountForAllowedJob(
        payload_records_.rbegin()->sequence_information().priority(),
        payload_records_.rbegin()->sequence_information().generation_id(),
        payload_records_.rbegin()->sequence_information().sequencing_id());
  }
}

TEST_F(EncryptedReportingClientTest, SecurityUploadsSequenceNotThrottled) {
  const size_t kTotalRetries = 10;

  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  for (size_t i = 0; i < kTotalRetries; ++i) {
    // Add one more SECURITY record for upload.
    AddRecordToPayload(Priority::SECURITY);
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());

    // New SECURITY event upload is allowed immediately.
    EXPECT_FALSE(
        encrypted_reporting_client
            ->WhenIsAllowedToProceed(
                payload_records_.rbegin()->sequence_information().priority(),
                payload_records_.rbegin()
                    ->sequence_information()
                    .generation_id())
            .is_positive());

    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(
        enqueued_result.value(),
        ElementsAre(
            payload_records_.rbegin()->sequence_information().sequencing_id()));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    auto response = ResponseBuilder(std::move(request_body)).Build();
    ASSERT_TRUE(response.has_value());
    SimulateCustomResponseForRequest(/*index=*/0, std::move(response));

    base::IgnoreResult(GetAndValidateResponse(response_event));

    encrypted_reporting_client->AccountForAllowedJob(
        payload_records_.rbegin()->sequence_information().priority(),
        payload_records_.rbegin()->sequence_information().generation_id(),
        payload_records_.rbegin()->sequence_information().sequencing_id());
  }
}

TEST_F(EncryptedReportingClientTest, FailedUploadsSequenceThrottled) {
  const size_t kTotalRetries = 10;

  auto encrypted_reporting_client = EncryptedReportingClient::Create(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client->PresetUploads(context_.Clone(), kDmToken,
                                            kClientId);

  std::list<int64_t> expected_cached_seq_ids;
  for (size_t i = 0; i < kTotalRetries; ++i) {
    // Add one more record for upload.
    AddRecordToPayload();
    ScopedReservation scoped_reservation(RecordsSize(payload_records_),
                                         memory_resource_);
    ASSERT_TRUE(scoped_reservation.reserved());
    expected_cached_seq_ids.push_back(
        payload_records_.rbegin()->sequence_information().sequencing_id());

    auto allowed_delay = encrypted_reporting_client->WhenIsAllowedToProceed(
        payload_records_.rbegin()->sequence_information().priority(),
        payload_records_.rbegin()->sequence_information().generation_id());
    if (i == 0) {
      // The very first upload is allowed.
      EXPECT_FALSE(allowed_delay.is_positive());
    } else {
      // Delay after permanent error is 1 day.
      EXPECT_THAT(allowed_delay, Ge(base::Days(1)));
      // Move forward to allow.
      task_environment_.FastForwardBy(allowed_delay - base::Seconds(1));
      EXPECT_TRUE(
          encrypted_reporting_client
              ->WhenIsAllowedToProceed(
                  payload_records_.rbegin()->sequence_information().priority(),
                  payload_records_.rbegin()
                      ->sequence_information()
                      .generation_id())
              .is_positive());
      task_environment_.FastForwardBy(base::Seconds(1));
    }

    EXPECT_FALSE(
        encrypted_reporting_client
            ->WhenIsAllowedToProceed(
                payload_records_.rbegin()->sequence_information().priority(),
                payload_records_.rbegin()
                    ->sequence_information()
                    .generation_id())
            .is_positive());

    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    test::TestEvent<StatusOr<UploadResponseParser>> response_event;
    encrypted_reporting_client->UploadReport(
        need_encryption_key_, config_file_version_, payload_records_,
        std::move(scoped_reservation), enqueued_event.cb(),
        response_event.cb());

    const auto& enqueued_result = enqueued_event.result();
    EXPECT_OK(enqueued_result);
    EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

    task_environment_.RunUntilIdle();

    // Simulate server-side processing, generate response.
    auto request_body = GetRequestBody(/*index=*/0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());
    const std::string& pending_request_url =
        (*url_loader_factory_.pending_requests())[0].request.url.spec();
    EXPECT_THAT(pending_request_url, StartsWith(kServerUrl));
    url_loader_factory_.SimulateResponseForPendingRequest(
        pending_request_url, "",
        /*status=*/::net::HTTP_UNAUTHORIZED);  // Permanent error.

    const auto& actual_response = response_event.result();
    ASSERT_FALSE(actual_response.has_value());

    encrypted_reporting_client->AccountForAllowedJob(
        payload_records_.rbegin()->sequence_information().priority(),
        payload_records_.rbegin()->sequence_information().generation_id(),
        payload_records_.rbegin()->sequence_information().sequencing_id());
  }
}
}  // namespace reporting
