// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_oauth2_token_service_ash.h"

#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using FetchAccessTokenForDeviceAccountCallback = crosapi::
    DeviceOAuth2TokenServiceAsh::FetchAccessTokenForDeviceAccountCallback;

class DeviceOAuth2TokenServiceAshTest : public testing::Test {
 public:
  DeviceOAuth2TokenServiceAshTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {
    ash::CryptohomeMiscClient::InitializeFake();
    ash::FakeCryptohomeMiscClient::Get()->SetServiceIsAvailable(true);
    ash::FakeCryptohomeMiscClient::Get()->set_system_salt(
        ash::FakeCryptohomeMiscClient::GetStubSystemSalt());
    ash::SystemSaltGetter::Initialize();
    DeviceOAuth2TokenServiceFactory::Initialize(
        test_url_loader_factory_.GetSafeWeakWrapper(),
        g_browser_process->local_state());
  }

  ~DeviceOAuth2TokenServiceAshTest() override {
    DeviceOAuth2TokenServiceFactory::Shutdown();
    ash::SystemSaltGetter::Shutdown();
    ash::CryptohomeMiscClient::Shutdown();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  ScopedTestingLocalState scoped_testing_local_state_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(DeviceOAuth2TokenServiceAshTest, SingleRequest) {
  base::test::TestFuture<crosapi::mojom::AccessTokenResultPtr> waiter;
  std::unique_ptr<crosapi::DeviceOAuth2TokenServiceAsh> service =
      std::make_unique<crosapi::DeviceOAuth2TokenServiceAsh>();

  service->FetchAccessTokenForDeviceAccount(/*scopes=*/{},
                                            waiter.GetCallback());
  EXPECT_TRUE(waiter.Wait());
}

// Tests that passing an empty callback does not crash.
TEST_F(DeviceOAuth2TokenServiceAshTest, EmptyCallback) {
  std::unique_ptr<crosapi::DeviceOAuth2TokenServiceAsh> service =
      std::make_unique<crosapi::DeviceOAuth2TokenServiceAsh>();
  service->FetchAccessTokenForDeviceAccount(
      /*scopes=*/{}, FetchAccessTokenForDeviceAccountCallback());
  base::RunLoop().RunUntilIdle();
}

TEST_F(DeviceOAuth2TokenServiceAshTest, MultipleRequests) {
  constexpr int kRequestCount = 4;
  std::unique_ptr<crosapi::DeviceOAuth2TokenServiceAsh> service =
      std::make_unique<crosapi::DeviceOAuth2TokenServiceAsh>();

  for (int call_count = 0; call_count < kRequestCount; call_count++) {
    base::test::TestFuture<crosapi::mojom::AccessTokenResultPtr> waiter;
    service->FetchAccessTokenForDeviceAccount(/*scopes=*/{},
                                              waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());
  }
}

TEST_F(DeviceOAuth2TokenServiceAshTest, Cancel) {
  std::unique_ptr<crosapi::DeviceOAuth2TokenServiceAsh> service =
      std::make_unique<crosapi::DeviceOAuth2TokenServiceAsh>();
  base::MockCallback<FetchAccessTokenForDeviceAccountCallback> callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(0);
  service->FetchAccessTokenForDeviceAccount(/*scopes=*/{}, callback.Get());
  // Destroy `service` with an outstanding request. This should not crash and
  // not call the callback.
  service->FetchAccessTokenForDeviceAccount(/*scopes=*/{}, callback.Get());
  service.reset();
  base::RunLoop().RunUntilIdle();
}
