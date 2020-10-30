// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/test_ambient_client.h"

#include <utility>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/time/time.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

const char* kTestGaiaId = "0123456789";

constexpr base::TimeDelta kDefaultTokenExpirationDelay =
    base::TimeDelta::FromSeconds(60);

}  // namespace

TestAmbientClient::TestAmbientClient(
    device::TestWakeLockProvider* wake_lock_provider)
    : wake_lock_provider_(wake_lock_provider) {}

TestAmbientClient::~TestAmbientClient() = default;

bool TestAmbientClient::IsAmbientModeAllowed() {
  // Only enable ambient mode for primary user to test multi login.
  return Shell::Get()->session_controller()->IsUserPrimary();
}

void TestAmbientClient::RequestAccessToken(GetAccessTokenCallback callback) {
  pending_callback_ = std::move(callback);
}

scoped_refptr<network::SharedURLLoaderFactory>
TestAmbientClient::GetURLLoaderFactory() {
  // TODO: return fake URL loader facotry.
  return nullptr;
}

void TestAmbientClient::RequestWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  wake_lock_provider_->BindReceiver(std::move(receiver));
}

void TestAmbientClient::IssueAccessToken(const std::string& access_token,
                                         bool with_error) {
  if (!pending_callback_)
    return;

  if (with_error) {
    std::move(pending_callback_)
        .Run(/*gaia_id=*/std::string(),
             /*access_token=*/std::string(),
             /*expiration_time=*/base::Time::Now());
  } else {
    std::move(pending_callback_)
        .Run(kTestGaiaId, access_token,
             base::Time::Now() + kDefaultTokenExpirationDelay);
  }
}

bool TestAmbientClient::ShouldUseProdServer() {
  return false;
}

bool TestAmbientClient::IsAccessTokenRequestPending() const {
  return !!pending_callback_;
}

}  // namespace ash
