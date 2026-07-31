// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_impl.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

TEST(RemoteActorCredentialSharingServiceTest, FactoryCreatesInstance) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kRemoteActorCredentialSharing);

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  RemoteActorCredentialSharingService* service =
      RemoteActorCredentialSharingServiceFactory::GetForProfile(&profile);
  EXPECT_NE(service, nullptr);
}

TEST(RemoteActorCredentialSharingServiceTest, FactoryReturnsNullForIncognito) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  Profile* incognito_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  RemoteActorCredentialSharingService* service =
      RemoteActorCredentialSharingServiceFactory::GetForProfile(
          incognito_profile);
  EXPECT_EQ(service, nullptr);
}

class RemoteActorCredentialSharingServiceImplTest : public testing::Test {
 public:
  RemoteActorCredentialSharingServiceImplTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    service_ = std::make_unique<RemoteActorCredentialSharingServiceImpl>(
        identity_test_env_.identity_manager(), test_shared_loader_factory_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<RemoteActorCredentialSharingServiceImpl> service_;
};

TEST_F(RemoteActorCredentialSharingServiceImplTest, SharePasswordSuccess) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialSharingService::ShareParameters params;
  params.obfuscated_gaia_id = "12345";
  params.web_origin = "https://nike.com";
  params.password_client_tag_hash = "tag_hash";
  params.password_data.set_signon_realm("https://nike.com");
  params.password_data.set_origin("https://nike.com");
  params.password_data.set_username_value("alice");
  params.password_data.set_password_value("password");
  params.time_to_live = base::Minutes(10);
  params.agent_oauth_client_id = "agent_client_id";

  service_->SharePassword(params, future.GetCallback());

  // 1. Passbox access token request
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token1", base::Time::Max());

  // 2. Passbox update request
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* passbox_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(passbox_request);
  EXPECT_EQ(passbox_request->request.method, "PATCH");
  EXPECT_EQ(
      passbox_request->request.url.spec(),
      "https://passbox-pa.googleapis.com/v1/internalservices/"
      "AGENTIC_CREDENTIAL_MANAGER/owneridnamespaces/GOOGLE_USER_ID/ownerids/"
      "12345/externalservices/https%3A%2F%2Fnike.com/credentials/"
      "tag_hash?allow_missing=true");

  // Respond to Passbox with 200 OK
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      passbox_request->request.url.spec(), "{}");

  // 3. APS access token request
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token2", base::Time::Max());

  // 4. APS grant request
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* aps_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(aps_request);
  EXPECT_EQ(aps_request->request.method, "POST");
  EXPECT_EQ(aps_request->request.url.spec(),
            "https://agenticpermission.pa.googleapis.com/v1/"
            "permissions:update?allow_missing=true");

  // Respond to APS with 200 OK
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      aps_request->request.url.spec(), "{}");

  EXPECT_TRUE(future.Get());
}

TEST_F(RemoteActorCredentialSharingServiceImplTest,
       SharePasswordPassboxFailure) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialSharingService::ShareParameters params;
  params.obfuscated_gaia_id = "12345";
  params.web_origin = "https://nike.com";
  params.password_client_tag_hash = "tag_hash";
  params.password_data.set_signon_realm("https://nike.com");
  params.password_data.set_origin("https://nike.com");
  params.password_data.set_username_value("alice");
  params.password_data.set_password_value("password");
  params.time_to_live = base::Minutes(10);
  params.agent_oauth_client_id = "agent_client_id";

  service_->SharePassword(params, future.GetCallback());

  // 1. Passbox access token request
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token1", base::Time::Max());

  // 2. Passbox update request
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* passbox_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(passbox_request);

  // Respond to Passbox with 400 Bad Request
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      passbox_request->request.url.spec(), "", net::HTTP_BAD_REQUEST);

  // APS should NOT be called.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  EXPECT_FALSE(future.Get());
}

TEST_F(RemoteActorCredentialSharingServiceImplTest, SharePasswordAPSFailure) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialSharingService::ShareParameters params;
  params.obfuscated_gaia_id = "12345";
  params.web_origin = "https://nike.com";
  params.password_client_tag_hash = "tag_hash";
  params.password_data.set_signon_realm("https://nike.com");
  params.password_data.set_origin("https://nike.com");
  params.password_data.set_username_value("alice");
  params.password_data.set_password_value("password");
  params.time_to_live = base::Minutes(10);
  params.agent_oauth_client_id = "agent_client_id";

  service_->SharePassword(params, future.GetCallback());

  // 1. Passbox access token request
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token1", base::Time::Max());

  // 2. Passbox update request
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* passbox_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(passbox_request);

  // Respond to Passbox with 200 OK
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      passbox_request->request.url.spec(), "{}");

  // 3. APS access token request
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token2", base::Time::Max());

  // 4. APS grant request
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* aps_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(aps_request);

  // Respond to APS with 400 Bad Request
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      aps_request->request.url.spec(), "", net::HTTP_BAD_REQUEST);

  // Deletion should NOT be triggered on APS failure (rely on TTL).
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  EXPECT_FALSE(future.Get());
}

TEST_F(RemoteActorCredentialSharingServiceImplTest,
       SharePasswordNoPrimaryAccountCallsBackWithFalse) {
  base::test::TestFuture<bool> future;
  RemoteActorCredentialSharingService::ShareParameters params;
  params.obfuscated_gaia_id = "12345";
  params.web_origin = "https://nike.com";
  params.password_client_tag_hash = "tag_hash";
  params.password_data.set_signon_realm("https://nike.com");
  params.password_data.set_origin("https://nike.com");
  params.password_data.set_username_value("alice");
  params.password_data.set_password_value("password");
  params.time_to_live = base::Minutes(10);
  params.agent_oauth_client_id = "agent_client_id";

  service_->SharePassword(params, future.GetCallback());

  EXPECT_FALSE(future.Get());
}

struct InvalidParamsTestCase {
  std::string test_name;
  std::string obfuscated_gaia_id;
  std::string web_origin;
  std::string password_client_tag_hash;
  std::string agent_oauth_client_id;
};

class RemoteActorCredentialSharingServiceImplInvalidParamsTest
    : public RemoteActorCredentialSharingServiceImplTest,
      public testing::WithParamInterface<InvalidParamsTestCase> {};

TEST_P(RemoteActorCredentialSharingServiceImplInvalidParamsTest,
       SharePasswordInvalidParams) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  const InvalidParamsTestCase& tc = GetParam();
  base::test::TestFuture<bool> future;
  RemoteActorCredentialSharingService::ShareParameters params;
  params.obfuscated_gaia_id = tc.obfuscated_gaia_id;
  params.web_origin = tc.web_origin;
  params.password_client_tag_hash = tc.password_client_tag_hash;
  params.password_data.set_signon_realm("https://nike.com");
  params.password_data.set_origin("https://nike.com");
  params.password_data.set_username_value("alice");
  params.password_data.set_password_value("password");
  params.time_to_live = base::Minutes(10);
  params.agent_oauth_client_id = tc.agent_oauth_client_id;

  service_->SharePassword(params, future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    RemoteActorCredentialSharingServiceImplInvalidParamsTest,
    testing::Values(
        InvalidParamsTestCase{"EmptyAgentClientId", "12345", "https://nike.com",
                              "tag_hash", ""},
        InvalidParamsTestCase{"EmptyWebOrigin", "12345", "", "tag_hash",
                              "agent_client_id"},
        InvalidParamsTestCase{"EmptyClientTagHash", "12345", "https://nike.com",
                              "", "agent_client_id"},
        InvalidParamsTestCase{"EmptyGaiaId", "", "https://nike.com", "tag_hash",
                              "agent_client_id"}
    ),
    [](const testing::TestParamInfo<InvalidParamsTestCase>& info) {
      return info.param.test_name;
    });

}  // namespace password_manager
