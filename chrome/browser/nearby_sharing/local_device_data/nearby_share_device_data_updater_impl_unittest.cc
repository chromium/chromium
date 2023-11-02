// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater_impl.h"
#include "chrome/browser/nearby_sharing/proto/device_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const char kDeviceIdPrefix[] = "users/me/devices/";
const char kTestDeviceId[] = "test_device_id";
const char kTestContactEmail1[] = "test1@gmail.com";
const char kTestContactEmail2[] = "test2@gmail.com";
const char kTestCertificateId1[] = "cert_id_1";
const char kTestCertificateId2[] = "cert_id_2";
const char kTestPersonName[] = "person_name";
constexpr base::TimeDelta kTestTimeout = base::Minutes(123);

const std::vector<nearbyshare::proto::Contact>& TestContactList() {
  static const base::NoDestructor<std::vector<nearbyshare::proto::Contact>>
      list([] {
        nearbyshare::proto::Contact contact1;
        contact1.mutable_identifier()->set_account_name(kTestContactEmail1);
        nearbyshare::proto::Contact contact2;
        contact2.mutable_identifier()->set_account_name(kTestContactEmail2);
        return std::vector<nearbyshare::proto::Contact>{contact1, contact2};
      }());
  return *list;
}

const std::vector<nearbyshare::proto::PublicCertificate>&
TestCertificateList() {
  static const base::NoDestructor<
      std::vector<nearbyshare::proto::PublicCertificate>>
      list([] {
        nearbyshare::proto::PublicCertificate cert1;
        cert1.set_secret_id(kTestCertificateId1);
        nearbyshare::proto::PublicCertificate cert2;
        cert2.set_secret_id(kTestCertificateId2);
        return std::vector<nearbyshare::proto::PublicCertificate>{cert1, cert2};
      }());
  return *list;
}

const nearbyshare::proto::UpdateDeviceResponse& TestResponse() {
  static const base::NoDestructor<nearbyshare::proto::UpdateDeviceResponse>
      response([] {
        nearbyshare::proto::UpdateDeviceResponse response;
        response.set_person_name(kTestPersonName);
        return response;
      }());
  return *response;
}

void VerifyRequest(
    const absl::optional<std::vector<nearbyshare::proto::Contact>>&
        expected_contacts,
    const absl::optional<std::vector<nearbyshare::proto::PublicCertificate>>&
        expected_certificates,
    const nearbyshare::proto::UpdateDeviceRequest& request) {
  std::vector<std::string> field_mask{request.update_mask().paths().begin(),
                                      request.update_mask().paths().end()};

  EXPECT_EQ(static_cast<size_t>(expected_contacts.has_value()) +
                static_cast<size_t>(expected_certificates.has_value()),
            field_mask.size());

  EXPECT_EQ(expected_contacts.has_value(),
            base::Contains(field_mask, "contacts"));
  EXPECT_EQ(expected_certificates.has_value(),
            base::Contains(field_mask, "public_certificates"));

  EXPECT_EQ(std::string(kDeviceIdPrefix) + kTestDeviceId,
            request.device().name());

  if (expected_contacts) {
    ASSERT_EQ(static_cast<int>(expected_contacts->size()),
              request.device().contacts().size());
    for (size_t i = 0; i < expected_contacts->size(); ++i) {
      EXPECT_EQ((*expected_contacts)[i].SerializeAsString(),
                request.device().contacts()[i].SerializeAsString());
    }
  } else {
    EXPECT_TRUE(request.device().contacts().empty());
  }

  if (expected_certificates) {
    ASSERT_EQ(static_cast<int>(expected_certificates->size()),
              request.device().public_certificates().size());
    for (size_t i = 0; i < expected_certificates->size(); ++i) {
      EXPECT_EQ((*expected_certificates)[i].SerializeAsString(),
                request.device().public_certificates()[i].SerializeAsString());
    }
  } else {
    EXPECT_TRUE(request.device().public_certificates().empty());
  }
}

void VerifyResponse(
    const absl::optional<nearbyshare::proto::UpdateDeviceResponse>&
        expected_response,
    const absl::optional<nearbyshare::proto::UpdateDeviceResponse>& response) {
  if (expected_response) {
    ASSERT_TRUE(response);
    EXPECT_EQ(expected_response->SerializeAsString(),
              response->SerializeAsString());
  } else {
    EXPECT_FALSE(response);
  }
}

}  // namespace

class NearbyShareDeviceDataUpdaterImplTest : public ::testing::Test {
 protected:
  enum class UpdateDeviceRequestResult { kSuccess, kHttpFailure, kTimeout };

  NearbyShareDeviceDataUpdaterImplTest() = default;
  ~NearbyShareDeviceDataUpdaterImplTest() override = default;

  void SetUp() override {
    updater_ = NearbyShareDeviceDataUpdaterImpl::Factory::Create(
        kTestDeviceId, kTestTimeout, &fake_client_factory_);
  }

  void CallUpdateDeviceData(
      const absl::optional<std::vector<nearbyshare::proto::Contact>>& contacts,
      const absl::optional<std::vector<nearbyshare::proto::PublicCertificate>>&
          certificates) {
    updater_->UpdateDeviceData(
        contacts, certificates,
        base::BindOnce(&NearbyShareDeviceDataUpdaterImplTest::OnResult,
                       base::Unretained(this)));
  }

  void ProcessNextUpdateDeviceDataRequest(
      const absl::optional<std::vector<nearbyshare::proto::Contact>>&
          expected_contacts,
      const absl::optional<std::vector<nearbyshare::proto::PublicCertificate>>&
          expected_certificates,
      UpdateDeviceRequestResult result) {
    // Verify the next request.
    ASSERT_FALSE(fake_client_factory_.instances().empty());
    FakeNearbyShareClient* client = fake_client_factory_.instances().back();
    ASSERT_EQ(1u, client->update_device_requests().size());
    VerifyRequest(expected_contacts, expected_certificates,
                  client->update_device_requests()[0].request);

    // Send and verify the response.
    size_t num_responses = responses_.size();
    switch (result) {
      case UpdateDeviceRequestResult::kSuccess:
        std::move(client->update_device_requests()[0].callback)
            .Run(TestResponse());
        break;
      case UpdateDeviceRequestResult::kHttpFailure:
        std::move(client->update_device_requests()[0].error_callback)
            .Run(NearbyShareHttpError::kBadRequest);
        break;
      case UpdateDeviceRequestResult::kTimeout:
        FastForward(kTestTimeout);
        break;
    }
    EXPECT_EQ(num_responses + 1, responses_.size());

    VerifyResponse(result == UpdateDeviceRequestResult::kSuccess
                       ? absl::make_optional(TestResponse())
                       : absl::nullopt,
                   responses_.back());
  }

 private:
  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  // The callback passed into UpdateDeviceData().
  void OnResult(const absl::optional<nearbyshare::proto::UpdateDeviceResponse>&
                    response) {
    responses_.push_back(response);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::vector<absl::optional<nearbyshare::proto::UpdateDeviceResponse>>
      responses_;
  FakeNearbyShareClientFactory fake_client_factory_;
  std::unique_ptr<NearbyShareDeviceDataUpdater> updater_;
};

TEST_F(NearbyShareDeviceDataUpdaterImplTest, Success_NoParameters) {
  CallUpdateDeviceData(/*contacts=*/absl::nullopt,
                       /*certificates=*/absl::nullopt);
  ProcessNextUpdateDeviceDataRequest(
      /*expected_contacts=*/absl::nullopt,
      /*expected_certificates=*/absl::nullopt,
      UpdateDeviceRequestResult::kSuccess);
}

TEST_F(NearbyShareDeviceDataUpdaterImplTest, Success_AllParameters) {
  CallUpdateDeviceData(TestContactList(), TestCertificateList());
  ProcessNextUpdateDeviceDataRequest(TestContactList(), TestCertificateList(),
                                     UpdateDeviceRequestResult::kSuccess);
}

TEST_F(NearbyShareDeviceDataUpdaterImplTest, Success_OneParameter) {
  CallUpdateDeviceData(TestContactList(),
                       /*certificates=*/absl::nullopt);
  ProcessNextUpdateDeviceDataRequest(TestContactList(),
                                     /*expected_certificates=*/absl::nullopt,
                                     UpdateDeviceRequestResult::kSuccess);
}

TEST_F(NearbyShareDeviceDataUpdaterImplTest, Failure_Timeout) {
  CallUpdateDeviceData(TestContactList(), TestCertificateList());
  ProcessNextUpdateDeviceDataRequest(TestContactList(), TestCertificateList(),
                                     UpdateDeviceRequestResult::kTimeout);
}

TEST_F(NearbyShareDeviceDataUpdaterImplTest, Failure_HttpError) {
  CallUpdateDeviceData(TestContactList(), TestCertificateList());
  ProcessNextUpdateDeviceDataRequest(TestContactList(), TestCertificateList(),
                                     UpdateDeviceRequestResult::kHttpFailure);
}

TEST_F(NearbyShareDeviceDataUpdaterImplTest, QueuedRequests) {
  // Queue requests while waiting to process.
  CallUpdateDeviceData(/*contacts=*/absl::nullopt,
                       /*certificates=*/absl::nullopt);
  CallUpdateDeviceData(TestContactList(), TestCertificateList());
  CallUpdateDeviceData(/*contacts=*/absl::nullopt, TestCertificateList());

  // Requests are processed in the order they are received.
  ProcessNextUpdateDeviceDataRequest(
      /*expected_contacts=*/absl::nullopt,
      /*expected_certificates=*/absl::nullopt,
      UpdateDeviceRequestResult::kSuccess);
  ProcessNextUpdateDeviceDataRequest(TestContactList(), TestCertificateList(),
                                     UpdateDeviceRequestResult::kTimeout);
  ProcessNextUpdateDeviceDataRequest(/*expected_contacts=*/absl::nullopt,
                                     TestCertificateList(),
                                     UpdateDeviceRequestResult::kHttpFailure);
}
