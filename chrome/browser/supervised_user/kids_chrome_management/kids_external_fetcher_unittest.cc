// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_external_fetcher.h"

#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
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

template <typename Response>
class Receiver : public FetcherDelegate<Response> {
 public:
  const absl::optional<Response>& GetResponse() const { return response_; }
  const absl::optional<std::string>& GetResponseBody() const {
    return response_body_;
  }

 private:
  void OnSuccess(std::unique_ptr<Response> response) override {
    response_.emplace(*response);
  }
  void OnFailure(base::StringPiece response_body) override {
    response_body_.emplace(response_body);
  }
  void OnMalformedResponse(base::StringPiece response_body) override {
    response_body_.emplace(response_body);
  }

  absl::optional<Response> response_;
  absl::optional<std::string> response_body_;
};

TEST_F(KidsExternalFetcherTest, AcceptsProtocolBufferRequests) {
  Receiver<ListFamilyMembersResponse> receiver;
  ListFamilyMembersRequest request;
  request.set_family_id("mine");
  ListFamilyMembersResponse response;
  response.set_self_obfuscated_gaia_id("gaia_id");

  auto fetcher = CreateListFamilyMembersFetcher(
      receiver, test_url_loader_factory_.GetSafeWeakWrapper());
  fetcher->StartRequest(request, "token", "http://example.com");

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

  base::ThreadPoolInstance::Get()->FlushForTesting();

  ASSERT_TRUE(receiver.GetResponse().has_value());
  EXPECT_EQ((*receiver.GetResponse()).self_obfuscated_gaia_id(), "gaia_id");
}

TEST_F(KidsExternalFetcherTest, HandlesMalformedResponse) {
  Receiver<ListFamilyMembersResponse> receiver;
  ListFamilyMembersRequest request;
  request.set_family_id("mine");

  auto fetcher = CreateListFamilyMembersFetcher(
      receiver, test_url_loader_factory_.GetSafeWeakWrapper());
  fetcher->StartRequest(request, "token", "http://example.com");

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
  EXPECT_FALSE(receiver.GetResponse().has_value());
}

TEST_F(KidsExternalFetcherTest, HandlesServerError) {
  Receiver<ListFamilyMembersResponse> receiver;
  ListFamilyMembersRequest request;
  request.set_family_id("mine");

  auto fetcher = CreateListFamilyMembersFetcher(
      receiver, test_url_loader_factory_.GetSafeWeakWrapper());
  fetcher->StartRequest(request, "token", "http://example.com");

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
  EXPECT_FALSE(receiver.GetResponse().has_value());
}

}  // namespace
}  // namespace chrome::kids
