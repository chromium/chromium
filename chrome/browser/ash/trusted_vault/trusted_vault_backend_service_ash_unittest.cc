// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/trusted_vault/trusted_vault_backend_service_ash.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "device/fido/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class TrustedVaultBackendServiceAshTest : public testing::Test {
 public:
  ~TrustedVaultBackendServiceAshTest() override = default;

  TrustedVaultBackendServiceAsh& backend_service() { return backend_service_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{device::kChromeOsPasskeys};

  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  trusted_vault::FakeTrustedVaultClient sync_client_ash_;
  trusted_vault::FakeTrustedVaultClient passkeys_client_ash_;
  TrustedVaultBackendServiceAsh backend_service_{
      identity_test_env_.identity_manager(), &sync_client_ash_,
      &passkeys_client_ash_};
};

TEST_F(TrustedVaultBackendServiceAshTest,
       ShouldDisconnectBackendRemotesOnShutdown) {
  mojo::Remote<crosapi::mojom::TrustedVaultBackend> sync_remote;
  backend_service().GetTrustedVaultBackend(
      crosapi::mojom::SecurityDomainId::kChromeSync,
      sync_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(sync_remote.is_connected());
  mojo::Remote<crosapi::mojom::TrustedVaultBackend> passkeys_remote;
  backend_service().GetTrustedVaultBackend(
      crosapi::mojom::SecurityDomainId::kPasskeys,
      passkeys_remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(passkeys_remote.is_connected());

  backend_service().Shutdown();
  sync_remote.FlushForTesting();
  EXPECT_FALSE(sync_remote.is_connected());
  passkeys_remote.FlushForTesting();
  EXPECT_FALSE(passkeys_remote.is_connected());
}

}  // namespace ash
