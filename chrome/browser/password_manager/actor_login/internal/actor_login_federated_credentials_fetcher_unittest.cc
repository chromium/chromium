// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_federated_credentials_fetcher.h"

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace actor_login {

namespace {

class MockIdentityCredentialSource
    : public content::webid::IdentityCredentialSource {
 public:
  MockIdentityCredentialSource() = default;
  ~MockIdentityCredentialSource() override = default;

  MOCK_METHOD(void,
              GetIdentityCredentialSuggestions,
              (const std::vector<GURL>&,
               GetIdentityCredentialSuggestionsCallback),
              (override));
  MOCK_METHOD(bool,
              SelectAccount,
              (const url::Origin&, const std::string&),
              (override));
  MOCK_METHOD(void,
              SetEmbedderLoginRequest,
              (const url::Origin&,
               const std::string&,
               base::OnceCallback<void(content::webid::FederatedLoginResult)>),
              (override));
  MOCK_METHOD(bool, HasPendingRequest, (), (override));
};

scoped_refptr<content::IdentityRequestAccount> CreateTestIdentityRequestAccount(
    const std::string& email,
    const std::string& idp_config_url) {
  content::IdentityProviderMetadata idp_metadata;
  idp_metadata.config_url = GURL(idp_config_url);
  idp_metadata.idp_login_url = GURL("https://idp.com/login");
  idp_metadata.brand_decoded_icon =
      gfx::Image::CreateFrom1xBitmap(gfx::test::CreateBitmap(56, 78));

  content::ClientMetadata client_metadata{GURL(), GURL(), GURL(), gfx::Image()};

  auto idp_data = base::MakeRefCounted<content::IdentityProviderData>(
      "idp", idp_metadata, client_metadata, blink::mojom::RpContext::kSignIn,
      std::nullopt,
      std::vector<content::IdentityRequestDialogDisclosureField>(), false);

  auto account = base::MakeRefCounted<content::IdentityRequestAccount>(
      /*id=*/"123", /*display_identifier=*/email,
      /*display_name=*/"Display Name",
      /*email=*/email, /*name=*/"Name", /*given_name=*/"Given Name",
      /*picture=*/GURL(), /*phone=*/"", /*username=*/"",
      std::vector<std::string>(), std::vector<std::string>(),
      std::vector<std::string>(), std::vector<std::string>());

  account->identity_provider = idp_data;
  account->decoded_picture =
      gfx::Image::CreateFrom1xBitmap(gfx::test::CreateBitmap(12, 34));
  return account;
}

}  // namespace

class ActorLoginFederatedCredentialsFetcherTest : public testing::Test {
 public:
  ActorLoginFederatedCredentialsFetcherTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  MockIdentityCredentialSource mock_identity_source_;
};

TEST_F(ActorLoginFederatedCredentialsFetcherTest, GetCredentialsSuccess) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFederatedLoginSupport);

  std::vector<scoped_refptr<content::IdentityRequestAccount>> accounts{
      CreateTestIdentityRequestAccount("test@example.com", "https://idp.com")};

  EXPECT_CALL(mock_identity_source_, GetIdentityCredentialSuggestions)
      .WillOnce(base::test::RunOnceCallback<1>(std::move(accounts)));

  base::test::TestFuture<std::vector<Credential>,
                         ActorLoginCredentialsFetcher::Status>
      future;
  url::Origin request_origin = url::Origin::Create(GURL("https://example.com"));
  ActorLoginFederatedCredentialsFetcher fetcher(
      request_origin, base::BindLambdaForTesting(
                          [&]() -> content::webid::IdentityCredentialSource* {
                            return &mock_identity_source_;
                          }));
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& [credentials, status] = future.Get();
  ASSERT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials[0].type, CredentialType::kFederated);
  EXPECT_EQ(credentials[0].username, u"test@example.com");
  EXPECT_EQ(credentials[0].source_site_or_app, u"idp");
  EXPECT_EQ(credentials[0].request_origin, request_origin);
  EXPECT_EQ(credentials[0].display_origin, u"example.com");
  ASSERT_TRUE(credentials[0].federation_detail.has_value());
  EXPECT_EQ(credentials[0].federation_detail->idp_origin,
            url::Origin::Create(GURL("https://idp.com")));
  EXPECT_EQ(credentials[0].federation_detail->account_picture.Size(),
            gfx::Size(12, 34));
  EXPECT_EQ(credentials[0].federation_detail->brand_icon.Size(),
            gfx::Size(56, 78));
  EXPECT_TRUE(credentials[0].immediatelyAvailableToLogin);
  EXPECT_EQ(status, ActorLoginCredentialsFetcher::Status::kSuccess);
}

TEST_F(ActorLoginFederatedCredentialsFetcherTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::kActorLoginFederatedLoginSupport);

  EXPECT_CALL(mock_identity_source_, GetIdentityCredentialSuggestions).Times(0);

  base::test::TestFuture<std::vector<Credential>,
                         ActorLoginCredentialsFetcher::Status>
      future;
  ActorLoginFederatedCredentialsFetcher fetcher(
      url::Origin::Create(GURL("https://example.com")),
      base::BindLambdaForTesting(
          [&]() -> content::webid::IdentityCredentialSource* {
            return &mock_identity_source_;
          }));
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& [credentials, status] = future.Get();
  EXPECT_TRUE(credentials.empty());
  EXPECT_EQ(status, ActorLoginCredentialsFetcher::Status::kSuccess);
}

TEST_F(ActorLoginFederatedCredentialsFetcherTest, NoAccounts) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFederatedLoginSupport);

  EXPECT_CALL(mock_identity_source_, GetIdentityCredentialSuggestions)
      .WillOnce(base::test::RunOnceCallback<1>(std::nullopt));

  base::test::TestFuture<std::vector<Credential>,
                         ActorLoginCredentialsFetcher::Status>
      future;
  ActorLoginFederatedCredentialsFetcher fetcher(
      url::Origin::Create(GURL("https://example.com")),
      base::BindLambdaForTesting(
          [&]() -> content::webid::IdentityCredentialSource* {
            return &mock_identity_source_;
          }));
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& [credentials, status] = future.Get();
  EXPECT_TRUE(credentials.empty());
  EXPECT_EQ(status, ActorLoginCredentialsFetcher::Status::kSuccess);
}

TEST_F(ActorLoginFederatedCredentialsFetcherTest, NoSource) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kActorLoginFederatedLoginSupport);

  base::test::TestFuture<std::vector<Credential>,
                         ActorLoginCredentialsFetcher::Status>
      future;
  ActorLoginFederatedCredentialsFetcher fetcher(
      url::Origin::Create(GURL("https://example.com")),
      base::BindLambdaForTesting(
          [&]() -> content::webid::IdentityCredentialSource* {
            return nullptr;
          }));
  fetcher.Fetch(future.GetCallback());

  ASSERT_TRUE(future.Wait());
  const auto& [credentials, status] = future.Get();
  EXPECT_TRUE(credentials.empty());
  EXPECT_EQ(status, ActorLoginCredentialsFetcher::Status::kSuccess);
}

}  // namespace actor_login
