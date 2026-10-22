// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_url_loader_header_client.h"

#include "base/run_loop.h"
#include "chrome/browser/enterprise/network_header_injection/http_header_injection_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_custom_headers {

namespace {

class MockTrustedURLLoaderHeaderClient
    : public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  MockTrustedURLLoaderHeaderClient() = default;
  ~MockTrustedURLLoaderHeaderClient() override = default;

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

  void Bind(mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
                receiver) {
    receiver_.Bind(std::move(receiver));
  }

 private:
  mojo::Receiver<network::mojom::TrustedURLLoaderHeaderClient> receiver_{this};
};

}  // namespace

class HttpHeaderInjectionURLLoaderHeaderClientTest : public testing::Test {
 public:
  HttpHeaderInjectionURLLoaderHeaderClientTest() = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Tests that `OnLoaderCreated` successfully forwards the request to the
// underlying target client, while keeping the intercepting client alive to
// inject headers.
TEST_F(HttpHeaderInjectionURLLoaderHeaderClientTest, ForwardsOnLoaderCreated) {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      target_remote;
  MockTrustedURLLoaderHeaderClient mock_target;
  mock_target.Bind(target_remote.InitWithNewPipeAndPassReceiver());

  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient> client_remote;
  HttpHeaderInjectionURLLoaderHeaderClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(&profile_)->GetWeakPtr(),
      client_remote.BindNewPipeAndPassReceiver(), std::move(target_remote));

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client;

  base::RunLoop run_loop;
  EXPECT_CALL(mock_target, OnLoaderCreated(123, testing::_)).WillOnce([&]() {
    run_loop.Quit();
  });

  client_remote->OnLoaderCreated(
      123, header_client.InitWithNewPipeAndPassReceiver());

  run_loop.Run();
}

// Tests that `OnLoaderForCorsPreflightCreated` successfully forwards the
// request to the underlying target client, ensuring CORS preflight requests are
// intercepted.
TEST_F(HttpHeaderInjectionURLLoaderHeaderClientTest,
       ForwardsOnLoaderForCorsPreflightCreated) {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      target_remote;
  MockTrustedURLLoaderHeaderClient mock_target;
  mock_target.Bind(target_remote.InitWithNewPipeAndPassReceiver());

  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient> client_remote;
  HttpHeaderInjectionURLLoaderHeaderClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(&profile_)->GetWeakPtr(),
      client_remote.BindNewPipeAndPassReceiver(), std::move(target_remote));

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client;
  network::ResourceRequest request;

  base::RunLoop run_loop;
  EXPECT_CALL(mock_target,
              OnLoaderForCorsPreflightCreated(testing::_, testing::_))
      .WillOnce([&]() { run_loop.Quit(); });

  client_remote->OnLoaderForCorsPreflightCreated(
      request, header_client.InitWithNewPipeAndPassReceiver());

  run_loop.Run();
}

// Tests that if the underlying target client disconnects prematurely, the
// `HttpHeaderInjectionURLLoaderHeaderClient` does not crash and continues to
// process requests so that enterprise headers can still be injected.
TEST_F(HttpHeaderInjectionURLLoaderHeaderClientTest, TargetDisconnected) {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      target_remote;
  auto mock_target = std::make_unique<MockTrustedURLLoaderHeaderClient>();
  mock_target->Bind(target_remote.InitWithNewPipeAndPassReceiver());

  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient> client_remote;
  HttpHeaderInjectionURLLoaderHeaderClient::Create(
      HttpHeaderInjectionServiceFactory::GetForProfile(&profile_)->GetWeakPtr(),
      client_remote.BindNewPipeAndPassReceiver(), std::move(target_remote));

  // Disconnect the target by destroying the mock which owns the receiver.
  mock_target.reset();

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client;

  // We should NOT crash when calling the client even though the target is
  // disconnected.
  client_remote->OnLoaderCreated(
      123, header_client.InitWithNewPipeAndPassReceiver());

  client_remote.FlushForTesting();
}

}  // namespace enterprise_custom_headers
