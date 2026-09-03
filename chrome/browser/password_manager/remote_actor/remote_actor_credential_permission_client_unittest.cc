// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_permission_client.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_switches.h"
#include "chrome/common/chrome_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class RemoteActorCredentialPermissionClientTest : public testing::Test {
 public:
  RemoteActorCredentialPermissionClientTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    client_ = std::make_unique<RemoteActorCredentialPermissionClient>(
        identity_test_env_.identity_manager(), test_shared_loader_factory_);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<RemoteActorCredentialPermissionClient> client_;
};

TEST_F(RemoteActorCredentialPermissionClientTest,
       GrantPasswordPermissionSuccess) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialPermissionClient::PasswordPermission permission;
  permission.task_id = "test_task_id";
  permission.web_origin = "https://nike.com";
  permission.password_client_tag_hash = "tag_hash";

  client_->GrantPasswordPermission(permission, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.method, "POST");
  EXPECT_EQ(pending_request->request.url.spec(),
            "https://agenticpermission.pa.googleapis.com/v1/"
            "permissions:update?allow_missing=true");

  std::string upload_data = network::GetUploadData(pending_request->request);
  auto body_value = base::test::ParseJson(upload_data);
  ASSERT_TRUE(body_value.is_dict());
  const base::DictValue& body_dict = body_value.GetDict();

  const base::DictValue* agent = body_dict.FindDict("agent");
  ASSERT_TRUE(agent);
  EXPECT_EQ(*agent->FindString("type"), "AGENT_TYPE_PERSONAL_ASSISTANT");
  EXPECT_EQ(*agent->FindString("agentOauthClientId"),
            "320695880279-gnq6the97ga85scn208u5jctnk82qelk.apps."
            "googleusercontent.com");

  const base::DictValue* delegation_context =
      body_dict.FindDict("delegationContext");
  ASSERT_TRUE(delegation_context);
  const base::DictValue* task_constraint =
      delegation_context->FindDict("taskConstraint");
  ASSERT_TRUE(task_constraint);
  const base::DictValue* task_id = task_constraint->FindDict("taskId");
  ASSERT_TRUE(task_id);
  EXPECT_EQ(task_id->FindInt("namespaceId").value_or(0), 30);
  EXPECT_EQ(*task_id->FindString("id"), "test_task_id");

  const base::DictValue* saved_passwords = body_dict.FindDict("savedPasswords");
  ASSERT_TRUE(saved_passwords);
  const base::ListValue* saved_password_access_list =
      saved_passwords->FindList("savedPasswordAccess");
  ASSERT_TRUE(saved_password_access_list);
  ASSERT_EQ(saved_password_access_list->size(), 1u);

  const base::DictValue& saved_password_access =
      (*saved_password_access_list)[0].GetDict();
  EXPECT_EQ(*saved_password_access.FindString("webOrigin"), "https://nike.com");
  EXPECT_TRUE(
      saved_password_access.FindBool("allAffiliatedPasswords").value_or(false));

  // Respond with 200 OK
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "{}");

  EXPECT_TRUE(future.Get());
}

TEST_F(RemoteActorCredentialPermissionClientTest,
       GrantPasswordPermissionWithCustomOAuthClientId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRemoteActorCredentialSharing,
      {{"oauth_client_id", "custom_client_id"}});

  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialPermissionClient::PasswordPermission permission;
  permission.task_id = "test_task_id";
  permission.web_origin = "https://nike.com";
  permission.password_client_tag_hash = "tag_hash";

  client_->GrantPasswordPermission(permission, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);

  std::string upload_data = network::GetUploadData(pending_request->request);
  auto body_value = base::test::ParseJson(upload_data);
  ASSERT_TRUE(body_value.is_dict());
  const base::DictValue* agent = body_value.GetDict().FindDict("agent");
  ASSERT_TRUE(agent);
  EXPECT_EQ(*agent->FindString("agentOauthClientId"), "custom_client_id");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "{}");

  EXPECT_TRUE(future.Get());
}

TEST_F(RemoteActorCredentialPermissionClientTest,
       GrantPasswordPermissionWithSwitchOverride) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kAgentPermissionServiceEndpoint, "https://custom-aps.com/");

  // Create a new client to pick up the command line switch.
  auto custom_client = std::make_unique<RemoteActorCredentialPermissionClient>(
      identity_test_env_.identity_manager(), test_shared_loader_factory_);

  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialPermissionClient::PasswordPermission permission;
  permission.task_id = "test_task_id";
  permission.web_origin = "https://nike.com";
  permission.password_client_tag_hash = "tag_hash";

  custom_client->GrantPasswordPermission(permission, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.method, "POST");
  EXPECT_EQ(pending_request->request.url.spec(),
            "https://custom-aps.com/v1/permissions:update?allow_missing=true");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "{}");

  EXPECT_TRUE(future.Get());
}

TEST_F(RemoteActorCredentialPermissionClientTest,
       GrantPasswordPermissionAccessTokenFetchFailure) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialPermissionClient::PasswordPermission permission;
  permission.task_id = "test_task_id";
  permission.web_origin = "https://nike.com";
  permission.password_client_tag_hash = "tag_hash";

  client_->GrantPasswordPermission(permission, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromServiceUnavailable(""));

  EXPECT_FALSE(future.Get());
}

TEST_F(RemoteActorCredentialPermissionClientTest,
       GrantPasswordPermissionHttpError) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialPermissionClient::PasswordPermission permission;
  permission.task_id = "test_task_id";
  permission.web_origin = "https://nike.com";
  permission.password_client_tag_hash = "tag_hash";

  client_->GrantPasswordPermission(permission, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);

  // Respond with 400 Bad Request
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "", net::HTTP_BAD_REQUEST);

  EXPECT_FALSE(future.Get());
}

TEST_F(RemoteActorCredentialPermissionClientTest,
       GrantPasswordPermissionOpaqueWebOrigin) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialPermissionClient::PasswordPermission permission;
  permission.task_id = "test_task_id";
  permission.web_origin = "";  // Opaque origin
  permission.password_client_tag_hash = "tag_hash";

  client_->GrantPasswordPermission(permission, future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(RemoteActorCredentialPermissionClientTest,
       GrantPasswordPermissionEmptyTaskId) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialPermissionClient::PasswordPermission permission;
  permission.task_id = "";
  permission.web_origin = "https://nike.com";
  permission.password_client_tag_hash = "tag_hash";

  client_->GrantPasswordPermission(permission, future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(RemoteActorCredentialPermissionClientTest,
       GrantPasswordPermissionEmptyClientId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRemoteActorCredentialSharing, {{"oauth_client_id", ""}});

  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  base::test::TestFuture<bool> future;
  RemoteActorCredentialPermissionClient::PasswordPermission permission;
  permission.task_id = "test_task_id";
  permission.web_origin = "https://nike.com";
  permission.password_client_tag_hash = "tag_hash";

  client_->GrantPasswordPermission(permission, future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

}  // namespace password_manager
