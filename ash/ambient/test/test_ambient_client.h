// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_TEST_AMBIENT_CLIENT_H_
#define ASH_AMBIENT_TEST_TEST_AMBIENT_CLIENT_H_

#include <string>

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"

namespace ash {

// An implementation for test support.
// IsAmbientModeAllowed() returns true to run the unittests.
class ASH_PUBLIC_EXPORT TestAmbientClient : public AmbientClient {
 public:
  explicit TestAmbientClient(device::TestWakeLockProvider* wake_lock_provider);
  ~TestAmbientClient() override;

  // AmbientClient:
  bool IsAmbientModeAllowed() override;
  void RequestAccessToken(GetAccessTokenCallback callback) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) override;
  bool ShouldUseProdServer() override;

  // Simulate to issue an |access_token|.
  // If |with_error| is true, will return an empty access token.
  void IssueAccessToken(const std::string& access_token, bool with_error);

  bool IsAccessTokenRequestPending() const;

 private:
  GetAccessTokenCallback pending_callback_;

  device::TestWakeLockProvider* const wake_lock_provider_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_TEST_AMBIENT_CLIENT_H_
