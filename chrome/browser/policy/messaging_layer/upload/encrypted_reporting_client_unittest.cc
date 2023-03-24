// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

using testing::Eq;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
};

TEST_F(EncryptedReportingClientTest, Default) {
  absl::optional<base::Value::Dict> actual_reponse;
  auto cb = base::BindLambdaForTesting(
      [&actual_reponse](absl::optional<base::Value::Dict> response) {
        actual_reponse = std::move(response);
      });

  EncryptedReportingClient encrypted_reporting_client(
      std::make_unique<FakeDelegate>(device_management_service_.get()));
  encrypted_reporting_client.UploadReport(std::move(merging_payload_),
                                          std::move(context_), kDmToken,
                                          kClientId, cb);
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(*url_loader_factory_.pending_requests(), SizeIs(1));

  const std::string& pending_request_url =
      (*url_loader_factory_.pending_requests())[0].request.url.spec();

  EXPECT_THAT(pending_request_url, StartsWith(kServerUrl));

  url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request_url,
      base::StringPrintf(R"({"%s" : "%s"})", kResponseKey, kResponseValue));

  ASSERT_TRUE(actual_reponse.has_value());
  ASSERT_THAT(actual_reponse.value(), SizeIs(1));
  ASSERT_TRUE(actual_reponse->FindString(kResponseKey));
  EXPECT_THAT(*(actual_reponse->FindString(kResponseKey)),
              StrEq(kResponseValue));

  DecrementSequenceId();
  BuildPayload();
  encrypted_reporting_client.UploadReport(std::move(merging_payload_),
                                          std::move(context_), kDmToken,
                                          kClientId, cb);

  // Sequence ID decreased, upload is rejected.
  EXPECT_FALSE(actual_reponse.has_value());
}

TEST_F(EncryptedReportingClientTest, ServiceUnavailable) {
  bool responded = false;
  absl::optional<base::Value::Dict> actual_reponse;

  EncryptedReportingClient encrypted_reporting_client(
      std::make_unique<FakeDelegate>(nullptr));
  encrypted_reporting_client.UploadReport(
      std::move(merging_payload_), std::move(context_), kDmToken, kClientId,
      base::BindLambdaForTesting(
          [&responded,
           &actual_reponse](absl::optional<base::Value::Dict> response) {
            responded = true;
            actual_reponse = std::move(response);
          }));

  ASSERT_TRUE(responded);
  EXPECT_FALSE(actual_reponse.has_value());
}

}  // namespace reporting
