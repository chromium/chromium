// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_TEST_AMBIENT_CLIENT_H_
#define ASH_AMBIENT_TEST_TEST_AMBIENT_CLIENT_H_

#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"

namespace network {
class TestURLLoaderFactory;
}  // namespace network

namespace ash {

// An implementation for test support.
// IsAmbientModeAllowed() returns true to run the unittests.
class ASH_PUBLIC_EXPORT TestAmbientClient : public AmbientClient {
 public:
  explicit TestAmbientClient(device::TestWakeLockProvider* wake_lock_provider);
  ~TestAmbientClient() override;

  static const char* kTestGaiaId;
  static const char* kTestAccessToken;

  // AmbientClient:
  bool IsAmbientModeAllowed() override;
  void SetAmbientModeAllowedForTesting(bool allowed) override {}
  void RequestAccessToken(GetAccessTokenCallback callback) override;
  void DownloadImage(const std::string& url,
                     ash::ImageDownloader::DownloadCallback callback) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSigninURLLoaderFactory()
      override;

  void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) override;
  bool ShouldUseProdServer() override;

  // Simulate to issue an |access_token|.
  // If |is_empty| is true, will return empty gaia id and access token,
  // otherwise returns |kTestGaiaId| and |kTestAccessToken|.
  void IssueAccessToken(bool is_empty);
  // If |is_automatic| is true, will automatically issue access token to all
  // requests. This helps simplify tests that do not care about auth tokens.
  void SetAutomaticalyIssueToken(bool is_automatic);

  bool IsAccessTokenRequestPending() const;

  network::TestURLLoaderFactory& test_url_loader_factory();

 private:
  bool is_automatic_ = false;
  GetAccessTokenCallback pending_callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<device::TestWakeLockProvider> wake_lock_provider_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_TEST_AMBIENT_CLIENT_H_
