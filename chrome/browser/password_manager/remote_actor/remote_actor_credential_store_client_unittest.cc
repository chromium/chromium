// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_store_client.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/common/time_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kTestGaiaId[] = "12345";
constexpr char kTestWebOrigin[] = "https://nike.com";
constexpr char kTestTagHash[] = "tag_hash";
constexpr char16_t kTestUsername[] = u"alice";
constexpr char16_t kTestPassword[] = u"password";

}  // namespace

class RemoteActorCredentialStoreClientTest : public testing::Test {
 public:
  RemoteActorCredentialStoreClientTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    store_ = std::make_unique<RemoteActorCredentialStoreClient>(
        identity_test_env_.identity_manager(), test_shared_loader_factory_);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<RemoteActorCredentialStoreClient> store_;
};

TEST_F(RemoteActorCredentialStoreClientTest, UpdateCredentialSuccess) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  store_->UpdateCredential(kTestGaiaId, kTestWebOrigin, kTestTagHash,
                           kTestUsername, kTestPassword, base::Minutes(10),
                           future.GetCallback());

  // 1. Access token request
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  // 2. Passbox PATCH request
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.method, "PATCH");
  EXPECT_EQ(pending_request->request.url.spec(),
            "https://passbox-pa.googleapis.com/v1/internalservices/"
            "AGENTIC_CREDENTIAL_MANAGER/owneridnamespaces/GOOGLE_USER_ID/"
            "ownerids/12345/externalservices/https%3A%2F%2Fnike.com/"
            "credentials/tag_hash?allow_missing=true");

  // Verify request body
  base::DictValue request_dict = base::test::ParseJsonDict(
      network::GetUploadData(pending_request->request));
  EXPECT_EQ(*request_dict.FindString("name"),
            "internalservices/AGENTIC_CREDENTIAL_MANAGER/owneridnamespaces/"
            "GOOGLE_USER_ID/ownerids/12345/externalservices/"
            "https%3A%2F%2Fnike.com/credentials/tag_hash");

  const base::DictValue* credential_data_dict =
      request_dict.FindDict("credentialData");
  ASSERT_TRUE(credential_data_dict);
  std::string base64_payload = *credential_data_dict->FindString("data");
  std::string json_payload;
  ASSERT_TRUE(base::Base64Decode(base64_payload, &json_payload));
  base::DictValue payload_dict = base::test::ParseJsonDict(json_payload);

  EXPECT_EQ(*payload_dict.FindString("signon_realm"), kTestWebOrigin);
  EXPECT_EQ(*payload_dict.FindString("username"),
            base::UTF16ToUTF8(kTestUsername));
  EXPECT_EQ(*payload_dict.FindString("password"),
            base::UTF16ToUTF8(kTestPassword));

  // Respond to Passbox with 200 OK
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "{}");

  EXPECT_TRUE(future.Get());
}

TEST_F(RemoteActorCredentialStoreClientTest,
       UpdateCredentialWithSwitchOverride) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kPassboxEndpoint, "https://custom-passbox.com/");

  auto custom_store = std::make_unique<RemoteActorCredentialStoreClient>(
      identity_test_env_.identity_manager(), test_shared_loader_factory_);

  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  custom_store->UpdateCredential(kTestGaiaId, kTestWebOrigin, kTestTagHash,
                                 kTestUsername, kTestPassword,
                                 base::Minutes(10), future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.method, "PATCH");
  EXPECT_EQ(pending_request->request.url.spec(),
            "https://custom-passbox.com/v1/internalservices/"
            "AGENTIC_CREDENTIAL_MANAGER/owneridnamespaces/GOOGLE_USER_ID/"
            "ownerids/12345/externalservices/https%3A%2F%2Fnike.com/"
            "credentials/tag_hash?allow_missing=true");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "{}");

  EXPECT_TRUE(future.Get());
}

TEST_F(RemoteActorCredentialStoreClientTest, DeleteCredentialSuccess) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  store_->DeleteCredential(kTestGaiaId, kTestWebOrigin, kTestTagHash,
                           future.GetCallback());

  // 1. Access token request
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.method, "DELETE");
  EXPECT_EQ(pending_request->request.url.spec(),
            "https://passbox-pa.googleapis.com/v1/internalservices/"
            "AGENTIC_CREDENTIAL_MANAGER/owneridnamespaces/GOOGLE_USER_ID/"
            "ownerids/12345/externalservices/https%3A%2F%2Fnike.com/"
            "credentials/tag_hash?allow_missing=true");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");

  EXPECT_TRUE(future.Get());
}

}  // namespace password_manager
