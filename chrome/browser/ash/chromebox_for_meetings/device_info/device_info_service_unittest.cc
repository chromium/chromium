// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/device_info/device_info_service.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_info.mojom.h"
#include "components/ownership/mock_owner_key_util.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::cfm {
namespace {

// TODO(https://crbug.com/1403174): Remove when namespace of mojoms for CfM are
// migarted to ash.
namespace mojom = ::chromeos::cfm::mojom;

constexpr char kReleaseVersion[] = "13671.0.2020";

class CfmDeviceInfoServiceTest : public ::testing::Test {
 public:
  CfmDeviceInfoServiceTest() = default;
  CfmDeviceInfoServiceTest(const CfmDeviceInfoServiceTest&) = delete;
  CfmDeviceInfoServiceTest& operator=(const CfmDeviceInfoServiceTest&) = delete;

  void SetUp() override {
    scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_(
        new ownership::MockOwnerKeyUtil());
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());
    DeviceSettingsService::Get()->SetSessionManager(&session_manager_client_,
                                                    owner_key_util_);

    CfmHotlineClient::InitializeFake();
    chromeos::cfm::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    DeviceInfoService::Initialize();
    system::StatisticsProvider::SetTestProvider(&fake_statistics_provider_);
  }

  void TearDown() override {
    DeviceInfoService::Shutdown();
    CfmHotlineClient::Shutdown();
    DeviceSettingsService::Get()->UnsetSessionManager();
  }

  FakeCfmHotlineClient* GetClient() {
    return static_cast<FakeCfmHotlineClient*>(CfmHotlineClient::Get());
  }

  void UpdatePolicyInfo(int64_t timestamp,
                        std::string directory_api_id,
                        std::string service_account_id,
                        int64_t gaia_id,
                        std::string cros_device_id) {
    device_policy_.policy_data().set_timestamp(timestamp);
    device_policy_.policy_data().set_directory_api_id(directory_api_id);
    device_policy_.policy_data().set_service_account_identity(
        service_account_id);
    device_policy_.policy_data().set_gaia_id(base::NumberToString(gaia_id));
    device_policy_.policy_data().set_device_id(cros_device_id);
    device_policy_.Build();
    session_manager_client_.set_device_policy(device_policy_.GetBlob());
    DeviceSettingsService::Get()->Load();
    content::RunAllTasksUntilIdle();
  }

  // Returns a mojo::Remote for the mojom::MeetDevicesInfo by faking the
  // way the cfm mojom binder daemon would request it through chrome.
  const mojo::Remote<mojom::MeetDevicesInfo>& GetDeviceInfoRemote() {
    if (device_info_remote_.is_bound()) {
      return device_info_remote_;
    }

    // if there is no valid remote create one
    auto* interface_name = mojom::MeetDevicesInfo::Name_;

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
        device_info_remote_.BindNewPipeAndPassReceiver().PassPipe());
    EXPECT_TRUE(device_info_remote_.is_connected());

    return device_info_remote_;
  }

 protected:
  chromeos::cfm::FakeCfmServiceContext context_;
  chromeos::cfm::FakeServiceConnectionImpl fake_service_connection_;
  ScopedTestDeviceSettingsService scoped_device_settings_service_;
  FakeSessionManagerClient session_manager_client_;
  system::FakeStatisticsProvider fake_statistics_provider_;
  mojo::ReceiverSet<mojom::CfmServiceContext> context_receiver_set_;
  mojo::Remote<mojom::CfmServiceAdaptor> adaptor_remote_;
  mojo::Remote<mojom::MeetDevicesInfo> device_info_remote_;
  policy::DevicePolicyBuilder device_policy_;

  // A device can become Cfm only if it's enterprise enrolled.
  ash::ScopedStubInstallAttributes test_install_attributes_{
      ash::StubInstallAttributes::CreateCloudManaged("domain", "device_id")};

  // Require a full task environment for testing device policy
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// This test ensures that the DiagnosticsInfoService is discoverable by its
// mojom name by sending a signal received by CfmHotlineClient.
// TODO(b/40766737): Fix tests broken due to use of Machine Statistics
// Crashes on linux-cfm-rel. crbug.com/1209841
TEST_F(CfmDeviceInfoServiceTest, DISABLED_InfoServiceAvailable) {
  ASSERT_TRUE(GetClient()->FakeEmitSignal(mojom::MeetDevicesInfo::Name_));
}

// This test ensures that the CfmDeviceInfoService correctly registers itself
// for discovery by the cfm mojom binder daemon and correctly returns a
// working mojom remote.
// TODO(b/40766737): Fix tests broken due to use of Machine Statistics
// Crashes on linux-cfm-rel. crbug.com/1209841
TEST_F(CfmDeviceInfoServiceTest, DISABLED_GetDeviceInfoRemote) {
  ASSERT_TRUE(GetDeviceInfoRemote().is_connected());
}

// TODO(b/40766737): Fix tests broken due to use of Machine Statistics
// Crashes on linux-cfm-rel. crbug.com/1209841
TEST_F(CfmDeviceInfoServiceTest, DISABLED_GetDeviceInfoService) {
  const auto& details_remote = GetDeviceInfoRemote();
  ASSERT_TRUE(details_remote.is_connected());
}

// TODO(b/40766737): Fix tests broken due to use of Machine Statistics
// Crashes on linux-cfm-rel. crbug.com/1209841
TEST_F(CfmDeviceInfoServiceTest, DISABLED_TestPolicyInfo) {
  base::RunLoop run_loop;
  const auto& details_remote = GetDeviceInfoRemote();
  run_loop.RunUntilIdle();

  int64_t timestamp = 10;
  std::string directory_api_id = "device_id";
  std::string service_account_id = "service_account_id";
  int64_t gaia_id = 20;
  std::string cros_device_id = "cros_device_id";
  UpdatePolicyInfo(timestamp, directory_api_id, service_account_id, gaia_id,
                   cros_device_id);

  base::RunLoop mojo_loop;
  details_remote->GetPolicyInfo(
      base::BindLambdaForTesting([&](mojom::PolicyInfoPtr policy_ptr) {
        ASSERT_EQ(timestamp, policy_ptr->timestamp_ms);
        ASSERT_EQ(directory_api_id, policy_ptr->device_id);
        ASSERT_EQ(service_account_id,
                  policy_ptr->service_account_email_address);
        ASSERT_EQ(gaia_id, policy_ptr->service_account_gaia_id);
        ASSERT_EQ(cros_device_id, policy_ptr->cros_device_id);
        mojo_loop.Quit();
      }));
  mojo_loop.Run();
}

// TODO(b/40766737): Fix tests broken due to use of Machine Statistics
// Crashes on linux-cfm-rel. crbug.com/1209841
TEST_F(CfmDeviceInfoServiceTest, DISABLED_TestSysInfo) {
  base::RunLoop run_loop;

  base::test::ScopedChromeOSVersionInfo version(
      base::StringPrintf("CHROMEOS_RELEASE_VERSION=%s\n", kReleaseVersion),
      base::Time::Now());

  const auto& details_remote = GetDeviceInfoRemote();
  run_loop.RunUntilIdle();

  base::RunLoop mojo_loop;
  details_remote->GetSysInfo(
      base::BindLambdaForTesting([&](mojom::SysInfoPtr info_ptr) {
        ASSERT_FALSE(info_ptr.is_null());
        EXPECT_EQ(info_ptr->release_version, kReleaseVersion);
        mojo_loop.Quit();
      }));
  mojo_loop.Run();
}

// TODO(b/40766737): Fix tests broken due to use of Machine Statistics
// Crashes on linux-cfm-rel. crbug.com/1209841
TEST_F(CfmDeviceInfoServiceTest, DISABLED_TestMachineStatisticsInfo) {
  const std::string kExpectedHwid = "kExpectedHwid";

  fake_statistics_provider_.SetMachineStatistic(system::kHardwareClassKey,
                                                kExpectedHwid);

  const auto& details_remote = GetDeviceInfoRemote();
  base::RunLoop().RunUntilIdle();

  base::RunLoop mojo_loop;
  details_remote->GetMachineStatisticsInfo(base::BindLambdaForTesting(
      [&](mojom::MachineStatisticsInfoPtr stats_ptr) {
        ASSERT_FALSE(stats_ptr.is_null());
        EXPECT_EQ(stats_ptr->hwid, kExpectedHwid);
        mojo_loop.Quit();
      }));
  mojo_loop.Run();
}

}  // namespace
}  // namespace ash::cfm
