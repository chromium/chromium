// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/mojom/network_service.mojom.h"

// clang-format off
#include <windows.h>  // Must be in front of other Windows header files.
#include <initguid.h>  // Must be in front of devpkey.h.
// Must be in front of Windows includes because they define LogSeverity and this
// breaks gmock.
#include "testing/gmock/include/gmock/gmock.h"
// clang-format on

#include <cfgmgr32.h>
#include <devpkey.h>
#include <newdev.h>
#include <ntddser.h>
#include <setupapi.h>
#include <shlobj.h>
#include <stdint.h>

#include <optional>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_devinfo.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "sandbox/policy/features.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void UninstallAllMatchingDevices(base::win::ScopedDevInfo dev_info) {
  SP_DEVINFO_DATA device_info_data = {};
  device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  DWORD member_index = 0;
  while (::SetupDiEnumDeviceInfo(dev_info.get(), member_index,
                                 &device_info_data)) {
    // Explicitly continue on failure, to make sure that all devices are
    // correctly removed.
    std::ignore = ::DiUninstallDevice(/*hwndParent=*/nullptr, dev_info.get(),
                                      &device_info_data, /*Flags=*/0,
                                      /*NeedReboot=*/nullptr);
    member_index++;
  }
}

std::optional<base::ScopedClosureRunner> InstallAdapter(
    const base::FilePath& inf,
    const std::wstring hwid) {
  GUID guid;
  wchar_t className[MAX_CLASS_NAME_LEN];

  if (!::SetupDiGetINFClass(inf.value().c_str(), &guid, className,
                            MAX_CLASS_NAME_LEN, 0)) {
    PLOG(ERROR) << "Unable to create SetupDiGetINFClass.";
    return std::nullopt;
  }

  base::win::ScopedDevInfo dev_info(
      ::SetupDiCreateDeviceInfoList(&guid, nullptr));
  if (!dev_info.is_valid()) {
    PLOG(ERROR) << "Unable to call SetupDiCreateDeviceInfoList.";
    return std::nullopt;
  }

  SP_DEVINFO_DATA deviceInfoData = {};
  deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

  if (!::SetupDiCreateDeviceInfo(dev_info.get(), className, &guid, nullptr,
                                 nullptr, DICD_GENERATE_ID, &deviceInfoData)) {
    PLOG(ERROR) << "Unable to call SetupDiCreateDeviceInfo";
    return std::nullopt;
  }

  if (!::SetupDiSetDeviceRegistryProperty(
          dev_info.get(), &deviceInfoData, SPDRP_HARDWAREID,
          reinterpret_cast<const BYTE*>(hwid.c_str()),
          (hwid.length() + 1) * sizeof(wchar_t))) {
    PLOG(ERROR) << "Unable to call SetupDiSetDeviceRegistryProperty.";
    return std::nullopt;
  }

  if (!::SetupDiCallClassInstaller(DIF_REGISTERDEVICE, dev_info.get(),
                                   &deviceInfoData)) {
    PLOG(ERROR) << "Unable to call SetupDiCallClassInstaller.";
    return std::nullopt;
  }

  BOOL reboot_required = FALSE;
  if (!::UpdateDriverForPlugAndPlayDevices(
          nullptr, hwid.c_str(), inf.value().c_str(), 0, &reboot_required)) {
    PLOG(ERROR) << "Unable to call UpdateDriverForPlugAndPlayDevices.";
    return std::nullopt;
  }

  return base::ScopedClosureRunner(
      base::BindOnce(&UninstallAllMatchingDevices, std::move(dev_info)));
}

class MockNetworkChangeManagerClient
    : public network::mojom::NetworkChangeManagerClient {
 public:
  MockNetworkChangeManagerClient(
      network::mojom::NetworkChangeManager* network_change_manager) {
    mojo::PendingRemote<network::mojom::NetworkChangeManagerClient>
        client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver());
    network_change_manager->RequestNotifications(std::move(client_remote));
  }

  MockNetworkChangeManagerClient(const MockNetworkChangeManagerClient&) =
      delete;
  MockNetworkChangeManagerClient& operator=(
      const MockNetworkChangeManagerClient&) = delete;

  ~MockNetworkChangeManagerClient() override {}

  // NetworkChangeManagerClient implementation:
  MOCK_METHOD(void,
              OnInitialConnectionType,
              (network::mojom::ConnectionType type),
              (override));
  MOCK_METHOD(void,
              OnNetworkChanged,
              (network::mojom::ConnectionType type),
              (override));

 private:
  mojo::Receiver<network::mojom::NetworkChangeManagerClient> receiver_{this};
};

}  // namespace

class SandboxedNetworkChangeNotifierBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface</*sandboxed=*/bool> {
 public:
  SandboxedNetworkChangeNotifierBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {sandbox::policy::features::kNetworkServiceSandbox,
           // When running inside the sandbox, the GetNetworkConnectivityHint
           // API must be used.
           net::features::kEnableGetNetworkConnectivityHintAPI},
          {features::kNetworkServiceInProcess});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kNetworkServiceInProcess,
               sandbox::policy::features::kNetworkServiceSandbox});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that dynamically adds a new adapter to the host, and verifies that a
// network change notification is sent from the network service to the browser
// process.
// The network service is able to see these network adapter changes, as it is
// created with the LPAC "internetClient" capability. See
// https://learn.microsoft.com/en-us/windows/uwp/packaging/app-capability-declarations
IN_PROC_BROWSER_TEST_P(SandboxedNetworkChangeNotifierBrowserTest,
                       AddNetworkAdapter) {
  if (!::IsUserAnAdmin()) {
    GTEST_SKIP() << "This test requires running elevated.";
  }
#if defined(ARCH_CPU_X86)
  if (!base::win::OSInfo::GetInstance()->IsWowDisabled()) {
    GTEST_SKIP()
        << "SetupDiCallClassInstaller can't be called from a 32 bit app"
        << " running in a 64 bit environment";
  }
#endif  // defined(ARCH_CPU_X86)

  mojo::Remote<network::mojom::NetworkChangeManager> network_change_manager;
  GetNetworkService()->GetNetworkChangeManager(
      network_change_manager.BindNewPipeAndPassReceiver());
  base::RunLoop run_loop;

  ::testing::StrictMock<MockNetworkChangeManagerClient> mock(
      network_change_manager.get());

  ::testing::InSequence order;
  // OnInitialConnectionType can be called with CONNECTION_UNKNOWN or
  // CONNECTION_ETHERNET.
  EXPECT_CALL(mock, OnInitialConnectionType(::testing::AnyOf(
                        network::mojom::ConnectionType::CONNECTION_UNKNOWN,
                        network::mojom::ConnectionType::CONNECTION_ETHERNET)));
  // NetworkChangeManager sends two notifications, the first is always
  // CONNECTION_NONE, followed by the actual ConnectionType. See
  // `network_change_manager.mojom`.
  EXPECT_CALL(
      mock, OnNetworkChanged(network::mojom::ConnectionType::CONNECTION_NONE));
  EXPECT_CALL(mock, OnNetworkChanged(
                        network::mojom::ConnectionType::CONNECTION_ETHERNET))
      .WillOnce([&run_loop]() { run_loop.Quit(); });

  // Install a new network card.
  base::FilePath dir_windows;
  ASSERT_TRUE(base::PathService::Get(base::DIR_WINDOWS, &dir_windows));
  auto inst = InstallAdapter(
      dir_windows.AppendASCII("Inf").AppendASCII("netloop.inf"), L"*MSLOOP");
  ASSERT_TRUE(inst);

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(,
                         SandboxedNetworkChangeNotifierBrowserTest,
                         ::testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "Sandboxed" : "Unsandboxed";
                         });
}  // namespace content
