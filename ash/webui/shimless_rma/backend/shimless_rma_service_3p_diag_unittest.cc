// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_service.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/webui/shimless_rma/backend/fake_shimless_rma_delegate.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom-shared.h"
#include "ash/webui/shimless_rma/mojom/shimless_rma.mojom.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/rmad/fake_rmad_client.h"
#include "chromeos/ash/components/dbus/rmad/rmad_client.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  }

  void TearDown() override {
    fake_shimless_rma_delegate_ = nullptr;
    shimless_rma_provider_.reset();
    RmadClient::Shutdown();
    cros_healthd::FakeCrosHealthd::Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  std::unique_ptr<ShimlessRmaService> shimless_rma_provider_;
  raw_ptr<FakeShimlessRmaDelegate> fake_shimless_rma_delegate_;
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

  base::test::TestFuture<const absl::optional<std::string>&> future;
  shimless_rma_provider_->Get3pDiagnosticsProvider(future.GetCallback());
  EXPECT_EQ(future.Get(), "TestOEMName");
}

TEST_F(ShimlessRmaService3pDiagTest,
       Get3pDiagnosticsProviderNotChromeOSSystemExtensionProvider) {
  SetFakeCrosHealthdOemName("TestOEMName");
  fake_shimless_rma_delegate_->set_is_chromeos_system_extension_provider(false);

  base::test::TestFuture<const absl::optional<std::string>&> future;
  shimless_rma_provider_->Get3pDiagnosticsProvider(future.GetCallback());
  EXPECT_EQ(future.Get(), absl::nullopt);
}

TEST_F(ShimlessRmaService3pDiagTest,
       Get3pDiagnosticsProviderFailedToGetOemName) {
  // Set empty cros_healthd response.
  auto info = ash::cros_healthd::mojom::TelemetryInfo::New();
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      info);

  base::test::TestFuture<const absl::optional<std::string>&> future;
  shimless_rma_provider_->Get3pDiagnosticsProvider(future.GetCallback());
  EXPECT_EQ(future.Get(), absl::nullopt);
}

}  // namespace
}  // namespace ash::shimless_rma
