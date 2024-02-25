// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/sharing/proto/contact_rpc.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace {

const char kTestDeviceId[] = "test_device_id";
const char kTestContactRecordId1[] = "contact_id_1";
const char kTestContactRecordId2[] = "contact_id_2";
const char kTestContactRecordId3[] = "contact_id_3";
const char kTestContactRecordId4[] = "contact_id_4";
const char kTestContactRecordId5[] = "contact_id_5";
const char kTestPageToken[] = "token";

constexpr base::TimeDelta kTestTimeout = base::Minutes(123);

const std::vector<nearby::sharing::proto::ContactRecord>&
TestContactRecordList() {
  static const base::NoDestructor<
      std::vector<nearby::sharing::proto::ContactRecord>>
      list([] {
        nearby::sharing::proto::ContactRecord contact1;
        contact1.set_id(kTestContactRecordId1);
        contact1.set_type(
            nearby::sharing::proto::ContactRecord::GOOGLE_CONTACT);
        contact1.set_is_reachable(true);
        nearby::sharing::proto::ContactRecord contact2;
        contact2.set_id(kTestContactRecordId2);
        contact2.set_type(
            nearby::sharing::proto::ContactRecord::DEVICE_CONTACT);
        contact2.set_is_reachable(true);
        nearby::sharing::proto::ContactRecord contact3;
        contact3.set_id(kTestContactRecordId3);
        contact3.set_type(nearby::sharing::proto::ContactRecord::UNKNOWN);
        contact3.set_is_reachable(true);
        nearby::sharing::proto::ContactRecord contact4;
        contact4.set_id(kTestContactRecordId4);
        contact4.set_type(
            nearby::sharing::proto::ContactRecord::GOOGLE_CONTACT);
        contact4.set_is_reachable(false);
        nearby::sharing::proto::ContactRecord contact5;
        contact5.set_id(kTestContactRecordId5);
        contact5.set_type(
            nearby::sharing::proto::ContactRecord::GOOGLE_CONTACT);
        contact5.set_is_reachable(false);
        return std::vector<nearby::sharing::proto::ContactRecord>{
            contact1, contact2, contact3, contact4, contact5};
      }());
  return *list;
}

nearby::sharing::proto::ListContactPeopleResponse
CreateListContactPeopleResponse(
    const std::vector<nearby::sharing::proto::ContactRecord>& contact_records,
    const std::optional<std::string>& next_page_token) {
  nearby::sharing::proto::ListContactPeopleResponse response;
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
    std::optional<std::vector<nearby::sharing::proto::ContactRecord>> contacts;
    std::optional<uint32_t> num_unreachable_contacts_filtered_out;
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
      const std::optional<std::string>& expected_page_token_in_request,
      const nearby::sharing::proto::ListContactPeopleResponse& response) {
    VerifyListContactPeopleRequest(expected_page_token_in_request);

    EXPECT_FALSE(result_);
    FakeNearbyShareClient* client = fake_client_factory_.instances().back();
    std::move(client->list_contact_people_requests()[0].callback).Run(response);
  }

  void FailListContactPeopleRequest(
      const std::optional<std::string>& expected_page_token_in_request) {
    VerifyListContactPeopleRequest(expected_page_token_in_request);

    EXPECT_FALSE(result_);
    FakeNearbyShareClient* client = fake_client_factory_.instances().back();
    std::move(client->list_contact_people_requests()[0].error_callback)
        .Run(ash::nearby::NearbyHttpError::kBadRequest);
  }

  void TimeoutListContactPeopleRequest(
      const std::optional<std::string>& expected_page_token_in_request) {
    VerifyListContactPeopleRequest(expected_page_token_in_request);

    EXPECT_FALSE(result_);
    FastForward(kTestTimeout);
  }

  void VerifySuccess(const std::vector<nearby::sharing::proto::ContactRecord>&
                         expected_contacts,
                     uint32_t expected_num_unreachable_contacts_filtered_out) {
    ASSERT_TRUE(result_);
    EXPECT_TRUE(result_->success);
    ASSERT_TRUE(result_->contacts);
    EXPECT_EQ(expected_num_unreachable_contacts_filtered_out,
              result_->num_unreachable_contacts_filtered_out);

    ASSERT_EQ(expected_contacts.size(), result_->contacts->size());
    for (size_t i = 0; i < expected_contacts.size(); ++i) {
      EXPECT_EQ(expected_contacts[i].SerializeAsString(),
                result_->contacts->at(i).SerializeAsString());
    }
  }

  void VerifyFailure() {
    ASSERT_TRUE(result_);
    EXPECT_FALSE(result_->success);
    EXPECT_FALSE(result_->contacts);
    EXPECT_FALSE(result_->num_unreachable_contacts_filtered_out);
  }

 private:
  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void VerifyListContactPeopleRequest(
      const std::optional<std::string>& expected_page_token) {
    ASSERT_FALSE(fake_client_factory_.instances().empty());
    FakeNearbyShareClient* client = fake_client_factory_.instances().back();
    ASSERT_EQ(1u, client->list_contact_people_requests().size());

    const nearby::sharing::proto::ListContactPeopleRequest& request =
        client->list_contact_people_requests()[0].request;
    EXPECT_EQ(expected_page_token.value_or(std::string()),
              request.page_token());
  }

  // The callbacks passed into NearbyShareContactDownloader ctor.
  void OnSuccess(std::vector<nearby::sharing::proto::ContactRecord> contacts,
                 uint32_t num_unreachable_contacts_filtered_out) {
    result_ = Result();
    result_->success = true;
    result_->contacts = std::move(contacts);
    result_->num_unreachable_contacts_filtered_out =
        num_unreachable_contacts_filtered_out;
  }
  void OnFailure() {
    result_ = Result();
    result_->success = false;
    result_->contacts.reset();
    result_->num_unreachable_contacts_filtered_out.reset();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::optional<Result> result_;
  FakeNearbyShareClientFactory fake_client_factory_;
  std::unique_ptr<NearbyShareContactDownloader> downloader_;
};

TEST_F(NearbyShareContactDownloaderImplTest, Success) {
  RunDownload();

  // Contacts are sent in two ListContactPeople responses.
  SucceedListContactPeopleRequest(
      /*expected_page_token=*/std::nullopt,
      CreateListContactPeopleResponse(
          std::vector<nearby::sharing::proto::ContactRecord>(
              TestContactRecordList().begin(),
              TestContactRecordList().begin() + 1),
          kTestPageToken));
  SucceedListContactPeopleRequest(
      /*expected_page_token=*/kTestPageToken,
      CreateListContactPeopleResponse(
          std::vector<nearby::sharing::proto::ContactRecord>(
              TestContactRecordList().begin() + 1,
              TestContactRecordList().end()),
          /*next_page_token=*/std::nullopt));

  // The last two records are filtered out because the are not reachable.
  VerifySuccess(/*expected_contacts=*/
                std::vector<nearby::sharing::proto::ContactRecord>(
                    TestContactRecordList().begin(),
                    TestContactRecordList().begin() + 3),
                /*expected_num_unreachable_contacts_filtered_out=*/2);
}

TEST_F(NearbyShareContactDownloaderImplTest, Failure_ListContactPeople) {
  RunDownload();

  // Contacts should be sent in two ListContactPeople responses, but second
  // request fails.
  SucceedListContactPeopleRequest(
      /*expected_page_token=*/std::nullopt,
      CreateListContactPeopleResponse(
          std::vector<nearby::sharing::proto::ContactRecord>(
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
      /*expected_page_token=*/std::nullopt,
      CreateListContactPeopleResponse(
          std::vector<nearby::sharing::proto::ContactRecord>(
              TestContactRecordList().begin(),
              TestContactRecordList().begin() + 1),
          kTestPageToken));
  TimeoutListContactPeopleRequest(
      /*expected_page_token=*/kTestPageToken);

  VerifyFailure();
}

TEST_F(NearbyShareContactDownloaderImplTest, Success_FilterOutDeviceContacts) {
  // Disable use of device contacts.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kNearbySharingDeviceContacts);

  RunDownload();

  SucceedListContactPeopleRequest(
      /*expected_page_token=*/std::nullopt,
      CreateListContactPeopleResponse(TestContactRecordList(),
                                      /*next_page_token=*/std::nullopt));

  // The device contact is filtered out.
  VerifySuccess(/*expected_contacts=*/{TestContactRecordList()[0],
                                       TestContactRecordList()[2]},
                /*expected_num_unreachable_contacts_filtered_out=*/2);
}
