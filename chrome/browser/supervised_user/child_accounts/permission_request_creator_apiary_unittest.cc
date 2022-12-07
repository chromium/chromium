// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEmail[] = "account@gmail.com";

std::string BuildResponse() {
  base::Value::Dict dict;
  base::Value::Dict permission_dict;
  permission_dict.Set("id", "requestid");
  dict.Set("permissionRequest", std::move(permission_dict));
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

}  // namespace

class PermissionRequestCreatorApiaryTest : public testing::Test {
 public:
  PermissionRequestCreatorApiaryTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSignin);
    account_id_ = account_info.account_id;
    permission_creator_ = std::make_unique<PermissionRequestCreatorApiary>(
        identity_test_env_.identity_manager(), test_shared_loader_factory_);
    permission_creator_->retry_on_network_change_ = false;
  }

 protected:
  void IssueAccessTokens() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        account_id_, "access_token", base::Time::Now() + base::Hours(1));
  }

  void IssueAccessTokenErrors() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        account_id_, GoogleServiceAuthError::FromServiceError("Error!"));
  }

  void SetupResponse(net::Error error, const std::string& response) {
    auto head = network::mojom::URLResponseHead::New();
    std::string headers("HTTP/1.1 200 OK\n\n");
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = response.size();
    test_url_loader_factory_.AddResponse(permission_creator_->GetApiUrl(),
                                         std::move(head), response, status);
  }

  void CreateRequest(const GURL& url) {
    permission_creator_->CreateURLAccessRequest(
        url,
        base::BindOnce(&PermissionRequestCreatorApiaryTest::OnRequestCreated,
                       base::Unretained(this)));
  }

  void WaitForResponse() { base::RunLoop().RunUntilIdle(); }

  MOCK_METHOD1(OnRequestCreated, void(bool success));

  base::test::SingleThreadTaskEnvironment task_environment_;
  CoreAccountId account_id_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<PermissionRequestCreatorApiary> permission_creator_;
};

TEST_F(PermissionRequestCreatorApiaryTest, Success) {
  CreateRequest(GURL("http://randomurl.com"));
  CreateRequest(GURL("http://anotherurl.com"));

  IssueAccessTokens();

  EXPECT_CALL(*this, OnRequestCreated(true)).Times(2);
  SetupResponse(net::OK, BuildResponse());
  SetupResponse(net::OK, BuildResponse());
  WaitForResponse();
}

TEST_F(PermissionRequestCreatorApiaryTest, AccessTokenError) {
  CreateRequest(GURL("http://randomurl.com"));

  // Our callback should get called immediately on an error.
  EXPECT_CALL(*this, OnRequestCreated(false));
  IssueAccessTokenErrors();
}

TEST_F(PermissionRequestCreatorApiaryTest, NetworkError) {
  const GURL& url = GURL("http://randomurl.com");
  CreateRequest(url);

  IssueAccessTokens();

  // Our callback should get called on an error.
  EXPECT_CALL(*this, OnRequestCreated(false));

  SetupResponse(net::ERR_ABORTED, std::string());
  WaitForResponse();
}
