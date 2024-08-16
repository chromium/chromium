// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/test_ambient_client.h"

#include <utility>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/time/time.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

constexpr base::TimeDelta kDefaultTokenExpirationDelay = base::Seconds(60);

// A simple SharedURLLoaderFactory implementation for tests.
class FakeSharedURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  FakeSharedURLLoaderFactory() = default;
  FakeSharedURLLoaderFactory(const FakeSharedURLLoaderFactory&) = delete;
  FakeSharedURLLoaderFactory& operator=(const FakeSharedURLLoaderFactory&) =
      delete;

  // network::mojom::URLLoaderFactory implementation:
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    test_url_loader_factory_.Clone(std::move(receiver));
  }

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    test_url_loader_factory_.CreateLoaderAndStart(
        std::move(loader), request_id, options, request, std::move(client),
        traffic_annotation);
  }

  // network::SharedURLLoaderFactory implementation:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED();
  }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  friend class base::RefCounted<FakeSharedURLLoaderFactory>;

  ~FakeSharedURLLoaderFactory() override = default;

  network::TestURLLoaderFactory test_url_loader_factory_;
};

}  // namespace

const char* TestAmbientClient::kTestGaiaId = "test_gaia_id";
const char* TestAmbientClient::kTestAccessToken = "test_access_token";

TestAmbientClient::TestAmbientClient(
    device::TestWakeLockProvider* wake_lock_provider)
    : url_loader_factory_(new FakeSharedURLLoaderFactory()),
      wake_lock_provider_(wake_lock_provider) {}

TestAmbientClient::~TestAmbientClient() = default;

bool TestAmbientClient::IsAmbientModeAllowed() {
  // Only enable ambient mode for primary user to test multi login.
  return Shell::Get()->session_controller()->IsUserPrimary();
}

void TestAmbientClient::RequestAccessToken(GetAccessTokenCallback callback) {
  pending_callback_ = std::move(callback);
  if (is_automatic_)
    IssueAccessToken(/*is_empty=*/false);
}

void TestAmbientClient::DownloadImage(
    const std::string& url,
    ash::ImageDownloader::DownloadCallback callback) {
  // Return fake image.
  std::move(callback).Run(gfx::test::CreateImageSkia(10, 10));
}

scoped_refptr<network::SharedURLLoaderFactory>
TestAmbientClient::GetURLLoaderFactory() {
  return url_loader_factory_;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestAmbientClient::GetSigninURLLoaderFactory() {
  return url_loader_factory_;
}

void TestAmbientClient::RequestWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  wake_lock_provider_->BindReceiver(std::move(receiver));
}

void TestAmbientClient::IssueAccessToken(bool is_empty) {
  if (!pending_callback_)
    return;

  if (is_empty) {
    std::move(pending_callback_)
        .Run(/*gaia_id=*/std::string(),
             /*access_token=*/std::string(),
             /*expiration_time=*/base::Time::Now());
  } else {
    std::move(pending_callback_)
        .Run(kTestGaiaId, kTestAccessToken,
             base::Time::Now() + kDefaultTokenExpirationDelay);
  }
}

void TestAmbientClient::SetAutomaticalyIssueToken(bool is_automatic) {
  is_automatic_ = is_automatic;
  if (is_automatic_)
    IssueAccessToken(/*is_empty=*/false);
}

bool TestAmbientClient::ShouldUseProdServer() {
  return false;
}

bool TestAmbientClient::IsAccessTokenRequestPending() const {
  return !!pending_callback_;
}

network::TestURLLoaderFactory& TestAmbientClient::test_url_loader_factory() {
  return static_cast<FakeSharedURLLoaderFactory*>(url_loader_factory_.get())
      ->test_url_loader_factory();
}

}  // namespace ash
