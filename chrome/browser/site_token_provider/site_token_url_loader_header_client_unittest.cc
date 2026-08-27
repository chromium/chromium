// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_url_loader_header_client.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace site_token_provider {

namespace {

using testing::_;
using testing::StrictMock;

class MockTrustedURLLoaderHeaderClient
    : public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  MockTrustedURLLoaderHeaderClient() = default;
  ~MockTrustedURLLoaderHeaderClient() override = default;

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(
      void,
      OnLoaderCreated,
      (int32_t request_id,
       mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver),
      (override));

  MOCK_METHOD(
      void,
      OnLoaderForCorsPreflightCreated,
      (const network::ResourceRequest& request,
       mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver),
      (override));

 private:
  mojo::Receiver<network::mojom::TrustedURLLoaderHeaderClient> receiver_{this};
};

}  // namespace

class SiteTokenURLLoaderHeaderClientTest : public testing::Test {
 public:
  SiteTokenURLLoaderHeaderClientTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSiteTokenProviderEnabled,
        {{"site_token_allowlist", "example.com"}});
    TestingProfile::Builder builder;
    profile_ = builder.Build();
  }

  TestingProfile* profile() { return profile_.get(); }

  SiteTokenProviderService* service() {
    return SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  }

  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient>
  CreateWrappedRemote(
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient> target =
          mojo::NullRemote()) {
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient> client =
        std::move(target);
    SiteTokenURLLoaderHeaderClient::MaybeWrap(profile(), &client);
    EXPECT_TRUE(client.is_valid());
    return mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient>(
        std::move(client));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(SiteTokenURLLoaderHeaderClientTest,
       MaybeWrapDoesNothingWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kSiteTokenProviderEnabled);

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  SiteTokenURLLoaderHeaderClient::MaybeWrap(profile(), &header_client);

  EXPECT_FALSE(header_client.is_valid());
}

TEST_F(SiteTokenURLLoaderHeaderClientTest,
       MaybeWrapDoesNothingWhenBrowserContextNull) {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  SiteTokenURLLoaderHeaderClient::MaybeWrap(nullptr, &header_client);

  EXPECT_FALSE(header_client.is_valid());
}

TEST_F(SiteTokenURLLoaderHeaderClientTest,
       MaybeWrapDoesNothingWhenServiceNull) {
  TestingProfile::Builder otr_builder;
  TestingProfile* otr_profile = otr_builder.BuildIncognito(profile());

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  SiteTokenURLLoaderHeaderClient::MaybeWrap(otr_profile, &header_client);

  EXPECT_FALSE(header_client.is_valid());
}

TEST_F(SiteTokenURLLoaderHeaderClientTest,
       MaybeWrapWrapsNullHeaderClientWhenFeatureEnabled) {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  SiteTokenURLLoaderHeaderClient::MaybeWrap(profile(), &header_client);

  EXPECT_TRUE(header_client.is_valid());
}

TEST_F(SiteTokenURLLoaderHeaderClientTest,
       MaybeWrapChainsExistingHeaderClient) {
  StrictMock<MockTrustedURLLoaderHeaderClient> mock_target;
  auto remote = CreateWrappedRemote(mock_target.BindAndGetRemote());

  base::test::TestFuture<void> future;
  EXPECT_CALL(mock_target, OnLoaderCreated(123, _))
      .WillOnce(
          [&](int32_t,
              mojo::PendingReceiver<network::mojom::TrustedHeaderClient>) {
            future.SetValue();
          });

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_remote;
  remote->OnLoaderCreated(123, header_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_TRUE(future.Wait());
}

TEST_F(SiteTokenURLLoaderHeaderClientTest, OnLoaderCreatedCreatesHeaderClient) {
  auto remote = CreateWrappedRemote();

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_remote;
  remote->OnLoaderCreated(456, header_remote.InitWithNewPipeAndPassReceiver());
  remote.FlushForTesting();

  mojo::Remote<network::mojom::TrustedHeaderClient> header_client(
      std::move(header_remote));
  header_client.FlushForTesting();
  EXPECT_TRUE(header_client.is_connected());
}

TEST_F(SiteTokenURLLoaderHeaderClientTest,
       OnLoaderForCorsPreflightCreatedStandaloneDoesNotInjectToken) {
  auto remote = CreateWrappedRemote();

  network::ResourceRequest request;
  request.url = GURL("https://example.com/cors");

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_remote;
  remote->OnLoaderForCorsPreflightCreated(
      request, header_remote.InitWithNewPipeAndPassReceiver());
  remote.FlushForTesting();

  mojo::Remote<network::mojom::TrustedHeaderClient> header_client(
      std::move(header_remote));
  header_client.FlushForTesting();
  // Receiver is dropped because tokens are not injected for CORS preflights.
  EXPECT_FALSE(header_client.is_connected());
}

TEST_F(SiteTokenURLLoaderHeaderClientTest,
       OnLoaderForCorsPreflightCreatedDelegatesToTarget) {
  StrictMock<MockTrustedURLLoaderHeaderClient> mock_target;
  auto remote = CreateWrappedRemote(mock_target.BindAndGetRemote());

  network::ResourceRequest request;
  request.url = GURL("https://example.com/api");

  base::test::TestFuture<void> future;
  EXPECT_CALL(
      mock_target,
      OnLoaderForCorsPreflightCreated(
          testing::Field(&network::ResourceRequest::url, request.url), _))
      .WillOnce(
          [&](const network::ResourceRequest&,
              mojo::PendingReceiver<network::mojom::TrustedHeaderClient>) {
            future.SetValue();
          });

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_remote;
  remote->OnLoaderForCorsPreflightCreated(
      request, header_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_TRUE(future.Wait());
}

TEST_F(SiteTokenURLLoaderHeaderClientTest, HandlesTargetDisconnectGracefully) {
  auto mock_target =
      std::make_unique<StrictMock<MockTrustedURLLoaderHeaderClient>>();
  auto remote = CreateWrappedRemote(mock_target->BindAndGetRemote());

  // Destroy the target client to simulate a disconnect.
  mock_target.reset();

  // Subsequent loader creation should not crash.
  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_remote;
  remote->OnLoaderCreated(789, header_remote.InitWithNewPipeAndPassReceiver());
  remote.FlushForTesting();

  EXPECT_TRUE(remote.is_connected());
}

}  // namespace site_token_provider
