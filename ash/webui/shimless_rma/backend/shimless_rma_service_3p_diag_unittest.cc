// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/webui/shimless_rma/backend/external_app_dialog.h"
#include "ash/webui/shimless_rma/backend/fake_shimless_rma_delegate.h"
#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom-shared.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/rmad/fake_rmad_client.h"
#include "chromeos/ash/components/dbus/rmad/rmad_client.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::shimless_rma {
namespace {

class ShimlessRmaService3pDiagTest : public ::testing::Test {
 public:
  void SetUp() override {
    cros_healthd::FakeCrosHealthd::Initialize();
    RmadClient::InitializeFake();
    auto delegate = std::make_unique<FakeShimlessRmaDelegate>();
    fake_shimless_rma_delegate_ = delegate.get();
    shimless_rma_provider_ =
        std::make_unique<ShimlessRmaService>(std::move(delegate));

    ExternalAppDialog::SetMockShowForTesting(base::BindLambdaForTesting(
        [&](const ExternalAppDialog::InitParams& params) {
          last_shown_url_ = params.content_url;
          last_shown_app_name_ = params.app_name;
        }));
  }

  void TearDown() override {
    ExternalAppDialog::SetMockShowForTesting(base::NullCallback());
    fake_shimless_rma_delegate_ = nullptr;
    shimless_rma_provider_.reset();
    RmadClient::Shutdown();
    cros_healthd::FakeCrosHealthd::Shutdown();
  }

  void SetFakePrepareDiagnosticsAppProfileResult(const std::string& iwa_id,
                                                 const std::string& app_name) {
    // This is only for passing between function and is never used in these
    // tests. Use a non-null value to pass the check.
    content::BrowserContext* fake_browser_context =
        reinterpret_cast<content::BrowserContext*>(1);
    fake_shimless_rma_delegate_->set_prepare_diagnostics_app_result(
        base::ok(ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult(
            fake_browser_context, "fake_extension_id",
            web_package::SignedWebBundleId::Create(iwa_id).value(), app_name,
            "Fake Permission 1\nFake Permission 2\n")));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  std::unique_ptr<ShimlessRmaService> shimless_rma_provider_;
  raw_ptr<FakeShimlessRmaDelegate> fake_shimless_rma_delegate_;
  GURL last_shown_url_;
  std::string last_shown_app_name_;
};

void SetFakeCrosHealthdOemName(const std::string& oem_name) {
  auto system_info = ash::cros_healthd::mojom::SystemInfo::New();
  system_info->os_info = ash::cros_healthd::mojom::OsInfo::New();
  system_info->os_info->os_version = ash::cros_healthd::mojom::OsVersion::New();
  system_info->os_info->oem_name = oem_name;
  auto telemetry_info = ash::cros_healthd::mojom::TelemetryInfo::New();
  telemetry_info->system_result =
      ash::cros_healthd::mojom::SystemResult::NewSystemInfo(
          std::move(system_info));
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      telemetry_info);
}

TEST_F(ShimlessRmaService3pDiagTest, Get3pDiagnosticsProvider) {
  SetFakeCrosHealthdOemName("TestOEMName");
  fake_shimless_rma_delegate_->set_is_chromeos_system_extension_provider(true);

  base::test::TestFuture<const std::optional<std::string>&> future;
  shimless_rma_provider_->Get3pDiagnosticsProvider(future.GetCallback());
  EXPECT_EQ(future.Get(), "TestOEMName");
}

TEST_F(ShimlessRmaService3pDiagTest,
       Get3pDiagnosticsProviderNotChromeOSSystemExtensionProvider) {
  SetFakeCrosHealthdOemName("TestOEMName");
  fake_shimless_rma_delegate_->set_is_chromeos_system_extension_provider(false);

  base::test::TestFuture<const std::optional<std::string>&> future;
  shimless_rma_provider_->Get3pDiagnosticsProvider(future.GetCallback());
  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(ShimlessRmaService3pDiagTest,
       Get3pDiagnosticsProviderFailedToGetOemName) {
  // Set empty cros_healthd response.
  auto info = ash::cros_healthd::mojom::TelemetryInfo::New();
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      info);

  base::test::TestFuture<const std::optional<std::string>&> future;
  shimless_rma_provider_->Get3pDiagnosticsProvider(future.GetCallback());
  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(ShimlessRmaService3pDiagTest, Show3pDiagnosticsAppNotInstalled) {
  base::test::TestFuture<mojom::Show3pDiagnosticsAppResult> future;
  shimless_rma_provider_->Show3pDiagnosticsApp(future.GetCallback());
  EXPECT_EQ(future.Get(), mojom::Show3pDiagnosticsAppResult::kAppNotInstalled);
}

ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult
MakeFakePrepareDiagnosticsAppProfileResult() {
  // This is only for passing between function and is never used in these
  // tests. Use a non-null value to pass the check.
  content::BrowserContext* fake_browser_context =
      reinterpret_cast<content::BrowserContext*>(1);
  auto fake_iwa_id =
      web_package::SignedWebBundleId::Create(
          "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic")
          .value();
  return ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult(
      fake_browser_context, "fake_extension_id", fake_iwa_id, "app_name",
      "Fake Permission 1\nFake Permission 2\n");
}

TEST_F(ShimlessRmaService3pDiagTest, Show3pDiagnosticsAppInstalled) {
  ash::FakeRmadClient::Get()->installed_diag_app_path() =
      base::FilePath{"/fake/diag/app"};
  auto fake_prepare_result = MakeFakePrepareDiagnosticsAppProfileResult();
  fake_shimless_rma_delegate_->set_prepare_diagnostics_app_result(
      base::ok(fake_prepare_result));
  {
    base::test::TestFuture<mojom::Show3pDiagnosticsAppResult> future;
    shimless_rma_provider_->Show3pDiagnosticsApp(future.GetCallback());
    EXPECT_EQ(future.Get(), mojom::Show3pDiagnosticsAppResult::kOk);
  }
  EXPECT_EQ(fake_shimless_rma_delegate_->last_load_crx_path(),
            base::FilePath{"/fake/diag/app.crx"});
  EXPECT_EQ(fake_shimless_rma_delegate_->last_load_swbn_path(),
            base::FilePath{"/fake/diag/app.swbn"});
  EXPECT_EQ(last_shown_url_,
            "isolated-app://" + fake_prepare_result.iwa_id.id());
  EXPECT_EQ(last_shown_app_name_, fake_prepare_result.name);

  // Try to show the app again. The second times will just reuse the loaded
  // profile so `PrepareDiagnosticsAppBrowserContext` should not be called.
  fake_shimless_rma_delegate_->set_prepare_diagnostics_app_result(
      base::unexpected(""));
  {
    base::test::TestFuture<mojom::Show3pDiagnosticsAppResult> future;
    shimless_rma_provider_->Show3pDiagnosticsApp(future.GetCallback());
    EXPECT_EQ(future.Get(), mojom::Show3pDiagnosticsAppResult::kOk);
  }
  EXPECT_EQ(last_shown_url_,
            "isolated-app://" + fake_prepare_result.iwa_id.id());
  EXPECT_EQ(last_shown_app_name_, fake_prepare_result.name);
}

TEST_F(ShimlessRmaService3pDiagTest, NoInstallable3pDiagApp) {
  base::test::TestFuture<const std::optional<base::FilePath>&> future;
  shimless_rma_provider_->GetInstallable3pDiagnosticsAppPath(
      future.GetCallback());
  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(ShimlessRmaService3pDiagTest, FoundInstallable3pDiagAppAndInstallIt) {
  ash::FakeRmadClient::Get()->external_diag_app_path() =
      base::FilePath{"/fake/external/diag/app"};
  auto fake_prepare_result = MakeFakePrepareDiagnosticsAppProfileResult();
  fake_shimless_rma_delegate_->set_prepare_diagnostics_app_result(
      base::ok(fake_prepare_result));

  {
    base::test::TestFuture<const std::optional<base::FilePath>&> future;
    shimless_rma_provider_->GetInstallable3pDiagnosticsAppPath(
        future.GetCallback());
    EXPECT_EQ(future.Get(), base::FilePath{"/fake/external/diag/app.swbn"});
  }
  {
    base::test::TestFuture<mojom::Shimless3pDiagnosticsAppInfoPtr> future;
    shimless_rma_provider_->InstallLastFound3pDiagnosticsApp(
        future.GetCallback());
    EXPECT_TRUE(future.Get());
    EXPECT_EQ(future.Get()->name, fake_prepare_result.name);
    EXPECT_EQ(future.Get()->permission_message,
              fake_prepare_result.permission_message);
    EXPECT_EQ(fake_shimless_rma_delegate_->last_load_crx_path(),
              base::FilePath{"/fake/external/diag/app.crx"});
    EXPECT_EQ(fake_shimless_rma_delegate_->last_load_swbn_path(),
              base::FilePath{"/fake/external/diag/app.swbn"});
  }
  {
    base::test::TestFuture<void> future;
    shimless_rma_provider_->CompleteLast3pDiagnosticsInstallation(
        /*is_approve=*/true, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(ash::FakeRmadClient::Get()->installed_diag_app_path(),
              base::FilePath{"/fake/external/diag/app"});
  }
  // Try to show the app. It should have already been loaded so
  // `PrepareDiagnosticsAppBrowserContext` should not be called.
  fake_shimless_rma_delegate_->set_prepare_diagnostics_app_result(
      base::unexpected(""));
  {
    base::test::TestFuture<mojom::Show3pDiagnosticsAppResult> future;
    shimless_rma_provider_->Show3pDiagnosticsApp(future.GetCallback());
    EXPECT_EQ(future.Get(), mojom::Show3pDiagnosticsAppResult::kOk);
  }
  EXPECT_EQ(last_shown_url_,
            "isolated-app://" + fake_prepare_result.iwa_id.id());
  EXPECT_EQ(last_shown_app_name_, fake_prepare_result.name);
}

TEST_F(ShimlessRmaService3pDiagTest,
       FoundInstallable3pDiagAppAndInstallItButNotApprove) {
  ash::FakeRmadClient::Get()->external_diag_app_path() =
      base::FilePath{"/fake/external/diag/app"};
  auto fake_prepare_result = MakeFakePrepareDiagnosticsAppProfileResult();
  fake_shimless_rma_delegate_->set_prepare_diagnostics_app_result(
      base::ok(fake_prepare_result));

  {
    base::test::TestFuture<const std::optional<base::FilePath>&> future;
    shimless_rma_provider_->GetInstallable3pDiagnosticsAppPath(
        future.GetCallback());
    EXPECT_EQ(future.Get(), base::FilePath{"/fake/external/diag/app.swbn"});
  }
  {
    base::test::TestFuture<mojom::Shimless3pDiagnosticsAppInfoPtr> future;
    shimless_rma_provider_->InstallLastFound3pDiagnosticsApp(
        future.GetCallback());
    EXPECT_TRUE(future.Get());
  }
  {
    base::test::TestFuture<void> future;
    shimless_rma_provider_->CompleteLast3pDiagnosticsInstallation(
        /*is_approve=*/false, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(ash::FakeRmadClient::Get()->installed_diag_app_path(),
              base::FilePath{});
  }
  {
    base::test::TestFuture<mojom::Show3pDiagnosticsAppResult> future;
    shimless_rma_provider_->Show3pDiagnosticsApp(future.GetCallback());
    EXPECT_EQ(future.Get(),
              mojom::Show3pDiagnosticsAppResult::kAppNotInstalled);
  }
}

}  // namespace
}  // namespace ash::shimless_rma
