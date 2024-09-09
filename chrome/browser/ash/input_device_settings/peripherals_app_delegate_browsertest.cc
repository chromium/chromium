// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/almanac_app_icon_loader.h"
#include "chrome/browser/ash/input_device_settings/peripherals_app_delegate_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const apps::PackageId kTestPackageId(apps::PackageType::kWeb,
                                     "test.package.name");
constexpr char kAppName[] = "app_name";
constexpr char kTestDeviceKey[] = "0000:0001";

}  // namespace

class PeripheralsAppDelegateImplTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();
    delegate_ = std::make_unique<PeripheralsAppDelegateImpl>();
    delegate()->set_profile_for_testing(profile_.get());
    ASSERT_TRUE(embedded_test_server()->Start());
    loader_ = std::make_unique<apps::AlmanacAppIconLoader>(*profile());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    loader_.reset();
    delegate_.reset();
    profile_.reset();
  }

  TestingProfile* profile() { return profile_.get(); }

  PeripheralsAppDelegateImpl* delegate() { return delegate_.get(); }

  network::TestURLLoaderFactory url_loader_factory_;

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PeripheralsAppDelegateImpl> delegate_;
  std::unique_ptr<apps::AlmanacAppIconLoader> loader_;
};

IN_PROC_BROWSER_TEST_F(PeripheralsAppDelegateImplTest,
                       GetCompanionAppResponse_Valid) {
  apps::proto::PeripheralsGetResponse response;
  response.set_package_id(kTestPackageId.ToString());
  response.set_name(kAppName);
  url_loader_factory_.AddResponse(delegate()->GetServerUrl().spec(),
                                  response.SerializeAsString(), net::HTTP_OK);
  base::test::TestFuture<const std::optional<mojom::CompanionAppInfo>&>
      app_info;
  delegate()->GetCompanionAppInfo(kTestDeviceKey, app_info.GetCallback());
  const auto info = app_info.Get();
  EXPECT_EQ(kAppName, info->app_name);
  EXPECT_EQ(kTestPackageId.ToString(), info->package_id);
}

IN_PROC_BROWSER_TEST_F(PeripheralsAppDelegateImplTest,
                       GetCompanionAppResponse_Invalid) {
  apps::proto::PeripheralsGetResponse response;
  url_loader_factory_.AddResponse(delegate()->GetServerUrl().spec(),
                                  response.SerializeAsString());
  base::test::TestFuture<const std::optional<mojom::CompanionAppInfo>&>
      app_info;
  delegate()->GetCompanionAppInfo(kTestDeviceKey, app_info.GetCallback());
  EXPECT_FALSE(app_info.Take().has_value());
}

}  // namespace ash
