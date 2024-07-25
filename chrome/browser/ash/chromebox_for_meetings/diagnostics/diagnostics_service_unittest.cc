// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/diagnostics/diagnostics_service.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_diagnostics.mojom.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::cfm {
namespace {

namespace mojom = ::chromeos::cfm::mojom;

class CfmDiagnosticsServiceTest : public ::testing::Test {
 public:
  CfmDiagnosticsServiceTest() = default;
  CfmDiagnosticsServiceTest(const CfmDiagnosticsServiceTest&) = delete;
  CfmDiagnosticsServiceTest& operator=(const CfmDiagnosticsServiceTest&) =
      delete;

  void SetUp() override {
    cros_healthd::FakeCrosHealthd::Initialize();
    CfmHotlineClient::InitializeFake();
    chromeos::cfm::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    DiagnosticsService::Initialize();
  }

  void TearDown() override {
    DiagnosticsService::Shutdown();
    CfmHotlineClient::Shutdown();
    cros_healthd::FakeCrosHealthd::Shutdown();
  }

  FakeCfmHotlineClient* GetClient() {
    return static_cast<FakeCfmHotlineClient*>(CfmHotlineClient::Get());
  }

  // Returns a mojo::Remote for the mojom::MeetDevicesDiagnostics by faking the
  // way the cfm mojom binder daemon would request it through chrome.
  const mojo::Remote<mojom::MeetDevicesDiagnostics>& GetDiagnosticsRemote() {
    if (diagnostics_remote_.is_bound()) {
      return diagnostics_remote_;
    }

    // if there is no valid remote create one
    auto* interface_name = mojom::MeetDevicesDiagnostics::Name_;

    base::RunLoop run_loop;

    // Fake out CfmServiceContext
    fake_service_connection_.SetCallback(base::BindLambdaForTesting(
        [&](mojo::PendingReceiver<mojom::CfmServiceContext> pending_receiver,
            bool success) {
          ASSERT_TRUE(success);
          context_receiver_set_.Add(&context_, std::move(pending_receiver));
        }));

    context_.SetFakeProvideAdaptorCallback(base::BindLambdaForTesting(
        [&](const std::string& service_id,
            mojo::PendingRemote<mojom::CfmServiceAdaptor>
                pending_adaptor_remote,
            mojom::CfmServiceContext::ProvideAdaptorCallback callback) {
          ASSERT_EQ(interface_name, service_id);
          adaptor_remote_.Bind(std::move(pending_adaptor_remote));
          std::move(callback).Run(true);
        }));

    EXPECT_TRUE(GetClient()->FakeEmitSignal(interface_name));
    run_loop.RunUntilIdle();

    EXPECT_TRUE(adaptor_remote_.is_connected());

    adaptor_remote_->OnBindService(
        diagnostics_remote_.BindNewPipeAndPassReceiver().PassPipe());
    EXPECT_TRUE(diagnostics_remote_.is_connected());

    return diagnostics_remote_;
  }

 protected:
  chromeos::cfm::FakeCfmServiceContext context_;
  mojo::Remote<mojom::MeetDevicesDiagnostics> diagnostics_remote_;
  mojo::ReceiverSet<mojom::CfmServiceContext> context_receiver_set_;
  mojo::Remote<mojom::CfmServiceAdaptor> adaptor_remote_;
  chromeos::cfm::FakeServiceConnectionImpl fake_service_connection_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

// This test ensures that the DiagnosticsInfoService is discoverable by its
// mojom name by sending a signal received by CfmHotlineClient.
TEST_F(CfmDiagnosticsServiceTest, InfoServiceAvailable) {
  ASSERT_TRUE(
      GetClient()->FakeEmitSignal(mojom::MeetDevicesDiagnostics::Name_));
}

// This test ensures that the CfmDeviceInfoService correctly registers itself
// for discovery by the cfm mojom binder daemon and correctly returns a
// working mojom remote.
TEST_F(CfmDiagnosticsServiceTest, GetDiagnosticsRemote) {
  ASSERT_TRUE(GetDiagnosticsRemote().is_connected());
}

TEST_F(CfmDiagnosticsServiceTest, GetDeviceInfoService) {
  const auto& details_remote = GetDiagnosticsRemote();
  ASSERT_TRUE(details_remote.is_connected());
}

// This test ensure that the diagnostics service can retrieve telemetry
// information from cros_healthd.
TEST_F(CfmDiagnosticsServiceTest, GetCrosHealthdTelemetry) {
  auto response = cros_healthd::mojom::TelemetryInfo::New();
  cros_healthd::FakeCrosHealthd::Get()->SetProbeTelemetryInfoResponseForTesting(
      response);
  base::RunLoop run_loop;
  DiagnosticsService::Get()->GetCrosHealthdTelemetry(base::BindLambdaForTesting(
      [&](cros_healthd::mojom::TelemetryInfoPtr info) {
        EXPECT_EQ(info, response);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test ensure that the diagnostics service can retrieve process-specific
// information from cros_healthd.
TEST_F(CfmDiagnosticsServiceTest, GetCrosHealthdProcessInfo) {
  auto response = cros_healthd::mojom::ProcessResult::NewProcessInfo(
      cros_healthd::mojom::ProcessInfo::New());
  cros_healthd::FakeCrosHealthd::Get()->SetProbeProcessInfoResponseForTesting(
      response);
  base::RunLoop run_loop;
  DiagnosticsService::Get()->GetCrosHealthdProcessInfo(
      /*pid=*/10, base::BindLambdaForTesting(
                      [&](cros_healthd::mojom::ProcessResultPtr info) {
                        EXPECT_EQ(info, response);
                        run_loop.Quit();
                      }));
  run_loop.Run();
}

}  // namespace
}  // namespace ash::cfm
