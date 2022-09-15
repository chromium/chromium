// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_external_fetcher.h"

#include <string>

#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chrome::kids {
namespace {

using ::kids_chrome_management::FamilyRole;
using ::kids_chrome_management::ListFamilyMembersRequest;
using ::kids_chrome_management::ListFamilyMembersResponse;
using ::network::GetUploadData;
using ::network::TestURLLoaderFactory;
using testing::Test;

// Tests the Kids External API fetchers functionality.
class KidsExternalFetcherTest : public Test {
 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::TaskEnvironment task_environment_;
};

template <typename Message>
Message ToProto(const std::string& input) {
  Message response;
  response.ParseFromString(input);
  return response;
}

// Convenience alias where the token value is know outright.
base::expected<signin::AccessTokenInfo, GoogleServiceAuthError> ToAccessToken(
    base::StringPiece access_token_value) {
  return signin::AccessTokenInfo(std::string(access_token_value),
                                 base::Time::Now() + base::Hours(1),
                                 "id_token");
}

// Convenience alias where the token error value is know outright.
base::expected<signin::AccessTokenInfo, GoogleServiceAuthError> ToAccessToken(
    GoogleServiceAuthError::State error) {
  return base::unexpected(GoogleServiceAuthError(error));
}

template <typename Request, typename Response>
class Receiver {
 public:
  using Error = typename Fetcher<Request, Response>::Error;
  const base::expected<Response, Error>& GetResult() const { return result_; }

  void Receive(Error error, Response response) {
    if (error != Error::NONE) {
      result_ = base::unexpected(error);
      return;
    }
    result_ = response;
  }

 private:
  base::expected<Response, Error> result_;
};

TEST_F(KidsExternalFetcherTest, AcceptsProtocolBufferRequests) {
  Receiver<ListFamilyMembersRequest, ListFamilyMembersResponse> receiver;
  ListFamilyMembersRequest request;
  request.set_family_id("mine");
  ListFamilyMembersResponse response;
  response.set_self_obfuscated_gaia_id("gaia_id");

  auto fetcher = CreateListFamilyMembersFetcher(
      test_url_loader_factory_.GetSafeWeakWrapper());
  fetcher->StartRequest(
      "http://example.com", request, ToAccessToken("token"),
      base::BindOnce(&Receiver<ListFamilyMembersRequest,
                               ListFamilyMembersResponse>::Receive,
                     base::Unretained(&receiver)));

  TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(nullptr, pending_request);
  EXPECT_EQ(
      ToProto<ListFamilyMembersRequest>(GetUploadData(pending_request->request))
          .family_id(),
      "mine");  // serialized proto.
  EXPECT_EQ(pending_request->request.url, "http://example.com/");
  EXPECT_EQ(pending_request->request.method, "POST");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "http://example.com/", response.SerializeAsString());

  ASSERT_TRUE(receiver.GetResult().has_value());
  EXPECT_EQ((*receiver.GetResult()).self_obfuscated_gaia_id(), "gaia_id");
}

TEST_F(KidsExternalFetcherTest, NoAccessToken) {
  Receiver<ListFamilyMembersRequest, ListFamilyMembersResponse> receiver;
  ListFamilyMembersRequest request;
  request.set_family_id("mine");

  auto fetcher = CreateListFamilyMembersFetcher(
      test_url_loader_factory_.GetSafeWeakWrapper());
  fetcher->StartRequest(
      "http://example.com", request,
      ToAccessToken(GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS),
      base::BindOnce(&Receiver<ListFamilyMembersRequest,
                               ListFamilyMembersResponse>::Receive,
                     base::Unretained(&receiver)));

  auto expected_error = Receiver<ListFamilyMembersRequest,
                                 ListFamilyMembersResponse>::Error::INPUT_ERROR;
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(receiver.GetResult().error(), expected_error);
}

TEST_F(KidsExternalFetcherTest, HandlesMalformedResponse) {
  Receiver<ListFamilyMembersRequest, ListFamilyMembersResponse> receiver;
  ListFamilyMembersRequest request;
  request.set_family_id("mine");

  auto fetcher = CreateListFamilyMembersFetcher(
      test_url_loader_factory_.GetSafeWeakWrapper());
  fetcher->StartRequest(
      "http://example.com", request, ToAccessToken("token"),
      base::BindOnce(&Receiver<ListFamilyMembersRequest,
                               ListFamilyMembersResponse>::Receive,
                     base::Unretained(&receiver)));

  TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(nullptr, pending_request);
  EXPECT_EQ(
      ToProto<ListFamilyMembersRequest>(GetUploadData(pending_request->request))
          .family_id(),
      "mine");  // serialized proto.
  EXPECT_EQ(pending_request->request.url, "http://example.com/");
  EXPECT_EQ(pending_request->request.method, "POST");

  std::string malformed_value("garbage");  // Not a valid marshaled proto.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "http://example.com/", malformed_value);
  EXPECT_FALSE(receiver.GetResult().has_value());
}

TEST_F(KidsExternalFetcherTest, HandlesServerError) {
  Receiver<ListFamilyMembersRequest, ListFamilyMembersResponse> receiver;
  ListFamilyMembersRequest request;
  request.set_family_id("mine");

  auto fetcher = CreateListFamilyMembersFetcher(
      test_url_loader_factory_.GetSafeWeakWrapper());
  fetcher->StartRequest(
      "http://example.com", request, ToAccessToken("token"),
      base::BindOnce(&Receiver<ListFamilyMembersRequest,
                               ListFamilyMembersResponse>::Receive,
                     base::Unretained(&receiver)));

  TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_NE(nullptr, pending_request);
  EXPECT_EQ(
      ToProto<ListFamilyMembersRequest>(GetUploadData(pending_request->request))
          .family_id(),
      "mine");  // serialized proto.
  EXPECT_EQ(pending_request->request.url, "http://example.com/");
  EXPECT_EQ(pending_request->request.method, "POST");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "http://example.com/", /*content=*/"", net::HTTP_BAD_REQUEST);
  EXPECT_FALSE(receiver.GetResult().has_value());
}

}  // namespace
}  // namespace chrome::kids
