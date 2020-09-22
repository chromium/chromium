// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"
#include "chrome/browser/nearby_sharing/proto/contact_rpc.pb.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestDeviceId[] = "test_device_id";
const char kTestContactRecordId1[] = "contact_id_1";
const char kTestContactRecordId2[] = "contact_id_2";
const char kTestContactRecordId3[] = "contact_id_3";
const char kTestPageToken[] = "token";

constexpr base::TimeDelta kTestTimeout = base::TimeDelta::FromMinutes(123);

const std::vector<nearbyshare::proto::ContactRecord>& TestContactRecordList() {
  static const base::NoDestructor<
      std::vector<nearbyshare::proto::ContactRecord>>
      list([] {
        nearbyshare::proto::ContactRecord contact1;
        contact1.set_id(kTestContactRecordId1);
        nearbyshare::proto::ContactRecord contact2;
        contact2.set_id(kTestContactRecordId2);
        nearbyshare::proto::ContactRecord contact3;
        contact3.set_id(kTestContactRecordId3);
        return std::vector<nearbyshare::proto::ContactRecord>{
            contact1, contact2, contact3};
      }());
  return *list;
}

nearbyshare::proto::ListContactPeopleResponse CreateListContactPeopleResponse(
    const std::vector<nearbyshare::proto::ContactRecord>& contact_records,
    const base::Optional<std::string>& next_page_token) {
  nearbyshare::proto::ListContactPeopleResponse response;
  *response.mutable_contact_records() = {contact_records.begin(),
                                         contact_records.end()};
  if (next_page_token)
    response.set_next_page_token(*next_page_token);

  return response;
}

}  // namespace

class NearbyShareContactDownloaderImplTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 protected:
  struct Result {
    bool success;
    base::Optional<std::vector<nearbyshare::proto::ContactRecord>> contacts;
  };

  NearbyShareContactDownloaderImplTest() = default;
  ~NearbyShareContactDownloaderImplTest() override = default;

  void RunDownload() {
    downloader_ = NearbyShareContactDownloaderImpl::Factory::Create(
        kTestDeviceId, kTestTimeout, &fake_client_factory_,
        base::BindOnce(&NearbyShareContactDownloaderImplTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&NearbyShareContactDownloaderImplTest::OnFailure,
                       base::Unretained(this)));
    downloader_->Run();
  }

  void SucceedListContactPeopleRequest(
      const base::Optional<std::string>& expected_page_token_in_request,
      const nearbyshare::proto::ListContactPeopleResponse& response) {
    VerifyListContactPeopleRequest(expected_page_token_in_request);

    EXPECT_FALSE(result_);
    FakeNearbyShareClient* client = fake_client_factory_.instances().back();
    std::move(client->list_contact_people_requests()[0].callback).Run(response);
  }

  void FailListContactPeopleRequest(
      const base::Optional<std::string>& expected_page_token_in_request) {
    VerifyListContactPeopleRequest(expected_page_token_in_request);

    EXPECT_FALSE(result_);
    FakeNearbyShareClient* client = fake_client_factory_.instances().back();
    std::move(client->list_contact_people_requests()[0].error_callback)
        .Run(NearbyShareHttpError::kBadRequest);
  }

  void TimeoutListContactPeopleRequest(
      const base::Optional<std::string>& expected_page_token_in_request) {
    VerifyListContactPeopleRequest(expected_page_token_in_request);

    EXPECT_FALSE(result_);
    FastForward(kTestTimeout);
  }

  void VerifySuccess(
      const std::vector<nearbyshare::proto::ContactRecord>& expected_contacts) {
    ASSERT_TRUE(result_);
    EXPECT_TRUE(result_->success);
    ASSERT_TRUE(result_->contacts);

    ASSERT_EQ(expected_contacts.size(), result_->contacts->size());
    for (size_t i = 0; i < expected_contacts.size(); ++i) {
      EXPECT_EQ(expected_contacts[i].SerializeAsString(),
                result_->contacts->at(i).SerializeAsString());
    }
  }

  void VerifyFailure() {
    ASSERT_TRUE(result_);
    EXPECT_FALSE(result_->success);
  }

 private:
  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void VerifyListContactPeopleRequest(
      const base::Optional<std::string>& expected_page_token) {
    ASSERT_FALSE(fake_client_factory_.instances().empty());
    FakeNearbyShareClient* client = fake_client_factory_.instances().back();
    ASSERT_EQ(1u, client->list_contact_people_requests().size());

    const nearbyshare::proto::ListContactPeopleRequest& request =
        client->list_contact_people_requests()[0].request;
    EXPECT_EQ(expected_page_token.value_or(std::string()),
              request.page_token());
  }

  // The callbacks passed into NearbyShareContactDownloader ctor.
  void OnSuccess(std::vector<nearbyshare::proto::ContactRecord> contacts) {
    result_ = Result();
    result_->success = true;
    result_->contacts = std::move(contacts);
  }
  void OnFailure() {
    result_ = Result();
    result_->success = false;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::Optional<Result> result_;
  FakeNearbyShareClientFactory fake_client_factory_;
  std::unique_ptr<NearbyShareContactDownloader> downloader_;
};

TEST_F(NearbyShareContactDownloaderImplTest, Success) {
  RunDownload();

  // Contacts are sent in two ListContactPeople responses.
  SucceedListContactPeopleRequest(
      /*expected_page_token=*/base::nullopt,
      CreateListContactPeopleResponse(
          std::vector<nearbyshare::proto::ContactRecord>(
              TestContactRecordList().begin(),
              TestContactRecordList().begin() + 1),
          kTestPageToken));
  SucceedListContactPeopleRequest(
      /*expected_page_token=*/kTestPageToken,
      CreateListContactPeopleResponse(
          std::vector<nearbyshare::proto::ContactRecord>(
              TestContactRecordList().begin() + 1,
              TestContactRecordList().end()),
          /*next_page_token=*/base::nullopt));

  VerifySuccess(/*expected_contacts=*/TestContactRecordList());
}

TEST_F(NearbyShareContactDownloaderImplTest, Failure_ListContactPeople) {
  RunDownload();

  // Contacts should be sent in two ListContactPeople responses, but second
  // request fails.
  SucceedListContactPeopleRequest(
      /*expected_page_token=*/base::nullopt,
      CreateListContactPeopleResponse(
          std::vector<nearbyshare::proto::ContactRecord>(
              TestContactRecordList().begin(),
              TestContactRecordList().begin() + 1),
          kTestPageToken));
  FailListContactPeopleRequest(
      /*expected_page_token=*/kTestPageToken);

  VerifyFailure();
}

TEST_F(NearbyShareContactDownloaderImplTest, Timeout_ListContactPeople) {
  RunDownload();

  // Contacts should be sent in two ListContactPeople responses. Timeout before
  // second response.
  SucceedListContactPeopleRequest(
      /*expected_page_token=*/base::nullopt,
      CreateListContactPeopleResponse(
          std::vector<nearbyshare::proto::ContactRecord>(
              TestContactRecordList().begin(),
              TestContactRecordList().begin() + 1),
          kTestPageToken));
  TimeoutListContactPeopleRequest(
      /*expected_page_token=*/kTestPageToken);

  VerifyFailure();
}
