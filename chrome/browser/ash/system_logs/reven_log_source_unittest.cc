// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/reven_log_source.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-shared.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace system_logs {

namespace {

namespace healthd = ::ash::cros_healthd::mojom;
using ::testing::HasSubstr;

constexpr char kRevenLogKey[] = "CHROMEOSFLEX_HARDWARE_INFO";

constexpr char kCpuNameKey[] = "cpu_name";
constexpr char kCpuNameVal[] = "Intel(R) Core(TM) i5-10210U CPU @ 1.60GHz";

constexpr char kTotalMemoryKey[] = "total_memory_kib";
constexpr char kFreeMemoryKey[] = "free_memory_kib";
constexpr char kAvailableMemoryKey[] = "available_memory_kib";
constexpr int kTotalMemory = 2048;
constexpr int kFreeMemory = 1024;
constexpr int kAvailableMemory = 512;

void SetCpuInfo(healthd::TelemetryInfoPtr& telemetry_info) {
  auto cpu_info = healthd::CpuInfo::New();
  auto physical_cpu_info = healthd::PhysicalCpuInfo::New();
  auto logical_cpu_info = healthd::LogicalCpuInfo::New();
  physical_cpu_info->logical_cpus.push_back(std::move(logical_cpu_info));
  physical_cpu_info->model_name = kCpuNameVal;
  cpu_info->num_total_threads = 2;
  cpu_info->physical_cpus.emplace_back(std::move(physical_cpu_info));

  telemetry_info->cpu_result =
      healthd::CpuResult::NewCpuInfo(std::move(cpu_info));
}

void SetCpuInfoWithProbeError(healthd::TelemetryInfoPtr& telemetry_info) {
  auto probe_error =
      healthd::ProbeError::New(healthd::ErrorType::kFileReadError, "");
  telemetry_info->cpu_result =
      healthd::CpuResult::NewError(std::move(probe_error));
}

void SetMemoryInfo(healthd::TelemetryInfoPtr& telemetry_info) {
  auto memory_info =
      healthd::MemoryInfo::New(kTotalMemory, kFreeMemory, kAvailableMemory, 0);
  telemetry_info->memory_result =
      healthd::MemoryResult::NewMemoryInfo(std::move(memory_info));
}

void SetMemoryInfoWithProbeError(healthd::TelemetryInfoPtr& telemetry_info) {
  auto probe_error =
      healthd::ProbeError::New(healthd::ErrorType::kFileReadError, "");
  telemetry_info->memory_result =
      healthd::MemoryResult::NewError(std::move(probe_error));
}

void SetSystemInfo(healthd::TelemetryInfoPtr& telemetry_info,
                   healthd::OsInfoPtr os_info,
                   healthd::DmiInfoPtr dmi_info) {
  auto system_info = healthd::SystemInfo::New();
  if (os_info) {
    system_info->os_info = std::move(os_info);
  }

  if (dmi_info) {
    system_info->dmi_info = std::move(dmi_info);
  }
  telemetry_info->system_result =
      healthd::SystemResult::NewSystemInfo(std::move(system_info));
}

healthd::OsInfoPtr CreateOsInfo(healthd::BootMode boot_mode) {
  healthd::OsInfoPtr os_info = healthd::OsInfo::New();
  os_info->os_version = healthd::OsVersion::New();
  os_info->boot_mode = boot_mode;
  return os_info;
}

healthd::DmiInfoPtr CreateDmiInfo() {
  healthd::DmiInfoPtr dmi_info = healthd::DmiInfo::New();
  dmi_info->sys_vendor = absl::optional<std::string>("LENOVO");
  dmi_info->product_name = absl::optional<std::string>("20U9001PUS");
  dmi_info->product_version =
      absl::optional<std::string>("ThinkPad X1 Carbon Gen 8");
  dmi_info->bios_version = absl::optional<std::string>("N2WET26W (1.16 )");
  return dmi_info;
}

healthd::BusDevicePtr CreatePciDevice(healthd::BusDeviceClass device_class,
                                      const std::string& vendor_name,
                                      const std::string& product_name,
                                      const std::string& driver,
                                      uint16_t vendor_id,
                                      uint16_t device_id) {
  auto device = healthd::BusDevice::New();
  device->device_class = device_class;
  device->vendor_name = vendor_name;
  device->product_name = product_name;

  auto pci_info = healthd::PciBusInfo::New();
  pci_info->vendor_id = vendor_id;
  pci_info->device_id = device_id;
  if (driver != "")
    pci_info->driver = absl::optional<std::string>(driver);

  device->bus_info = healthd::BusInfo::NewPciBusInfo(std::move(pci_info));
  return device;
}

void SetPciEthernetDevices(healthd::TelemetryInfoPtr& telemetry_info) {
  std::vector<healthd::BusDevicePtr> bus_devices;
  bus_devices.push_back(
      CreatePciDevice(healthd::BusDeviceClass::kEthernetController, "intel",
                      "product1", "driver1", 0x12ab, 0x34cd));
  bus_devices.push_back(
      CreatePciDevice(healthd::BusDeviceClass::kEthernetController, "broadcom",
                      "product2", "", 0x56ab, 0x78cd));

  telemetry_info->bus_result =
      healthd::BusResult::NewBusDevices(std::move(bus_devices));
}

void SetPciWirelessDevices(healthd::TelemetryInfoPtr& telemetry_info) {
  std::vector<healthd::BusDevicePtr> bus_devices;
  bus_devices.push_back(
      CreatePciDevice(healthd::BusDeviceClass::kWirelessController, "intel",
                      "wireless_product1", "wireless_driver1", 0x12ab, 0x34cd));
  bus_devices.push_back(
      CreatePciDevice(healthd::BusDeviceClass::kWirelessController, "broadcom",
                      "wireless_product2", "", 0x56ab, 0x78cd));

  telemetry_info->bus_result =
      healthd::BusResult::NewBusDevices(std::move(bus_devices));
}

void SetPciBluetoothDevices(healthd::TelemetryInfoPtr& telemetry_info) {
  std::vector<healthd::BusDevicePtr> bus_devices;
  bus_devices.push_back(CreatePciDevice(
      healthd::BusDeviceClass::kBluetoothAdapter, "intel", "bluetooth_product1",
      "bluetooth_driver1", 0x12ab, 0x34cd));
  bus_devices.push_back(
      CreatePciDevice(healthd::BusDeviceClass::kBluetoothAdapter, "broadcom",
                      "bluetooth_product2", "", 0x56ab, 0x78cd));

  telemetry_info->bus_result =
      healthd::BusResult::NewBusDevices(std::move(bus_devices));
}

void SetPciDisplayDevices(healthd::TelemetryInfoPtr& telemetry_info) {
  std::vector<healthd::BusDevicePtr> bus_devices;
  bus_devices.push_back(
      CreatePciDevice(healthd::BusDeviceClass::kDisplayController, "intel",
                      "945GM", "driver1", 0x12ab, 0x34cd));

  telemetry_info->bus_result =
      healthd::BusResult::NewBusDevices(std::move(bus_devices));
}

healthd::BusDevicePtr CreateUsbDevice(healthd::BusDeviceClass device_class,
                                      const std::string& vendor_name,
                                      const std::string& product_name,
                                      const std::string& driver,
                                      uint16_t vendor_id,
                                      uint16_t product_id) {
  auto device = healthd::BusDevice::New();
  device->device_class = device_class;
  device->vendor_name = vendor_name;
  device->product_name = product_name;

  auto usb_info = healthd::UsbBusInfo::New();
  usb_info->vendor_id = vendor_id;
  usb_info->product_id = product_id;
  auto usb_if_info = healthd::UsbBusInterfaceInfo::New();
  if (driver != "")
    usb_if_info->driver = absl::optional<std::string>(driver);
  usb_info->interfaces.push_back(std::move(usb_if_info));

  device->bus_info = healthd::BusInfo::NewUsbBusInfo(std::move(usb_info));
  return device;
}

void SetUsbEthernetDevices(healthd::TelemetryInfoPtr& telemetry_info) {
  std::vector<healthd::BusDevicePtr> bus_devices;
  bus_devices.push_back(
      CreateUsbDevice(healthd::BusDeviceClass::kEthernetController, "intel",
                      "product1", "driver1", 0x12ab, 0x34cd));
  bus_devices.push_back(
      CreateUsbDevice(healthd::BusDeviceClass::kEthernetController, "broadcom",
                      "product2", "", 0x56ab, 0x78cd));

  telemetry_info->bus_result =
      healthd::BusResult::NewBusDevices(std::move(bus_devices));
}

void SetUsbWirelessDevices(healthd::TelemetryInfoPtr& telemetry_info) {
  std::vector<healthd::BusDevicePtr> bus_devices;
  bus_devices.push_back(
      CreateUsbDevice(healthd::BusDeviceClass::kWirelessController, "intel",
                      "wireless_product1", "wireless_driver1", 0x12ab, 0x34cd));
  bus_devices.push_back(
      CreateUsbDevice(healthd::BusDeviceClass::kWirelessController, "broadcom",
                      "wireless_product2", "", 0x56ab, 0x78cd));

  telemetry_info->bus_result =
      healthd::BusResult::NewBusDevices(std::move(bus_devices));
}

void SetUsbBluetoothDevices(healthd::TelemetryInfoPtr& telemetry_info) {
  std::vector<healthd::BusDevicePtr> bus_devices;
  bus_devices.push_back(CreateUsbDevice(
      healthd::BusDeviceClass::kBluetoothAdapter, "intel", "bluetooth_product1",
      "bluetooth_driver1", 0x12ab, 0x34cd));
  bus_devices.push_back(
      CreateUsbDevice(healthd::BusDeviceClass::kBluetoothAdapter, "broadcom",
                      "bluetooth_product2", "", 0x56ab, 0x78cd));

  telemetry_info->bus_result =
      healthd::BusResult::NewBusDevices(std::move(bus_devices));
}

void SetUsbDisplayDevices(healthd::TelemetryInfoPtr& telemetry_info) {
  std::vector<healthd::BusDevicePtr> bus_devices;
  bus_devices.push_back(
      CreateUsbDevice(healthd::BusDeviceClass::kDisplayController, "intel",
                      "product1", "driver1", 0x12ab, 0x34cd));
  telemetry_info->bus_result =
      healthd::BusResult::NewBusDevices(std::move(bus_devices));
}

void SetTpmInfo(healthd::TelemetryInfoPtr& telemetry_info,
                const std::string& did_vid,
                uint32_t family,
                bool is_owned,
                bool is_allowed) {
  auto version = healthd::TpmVersion::New();
  version->family = family;
  version->manufacturer = 1129467731;
  version->spec_level = 116;

  auto status = healthd::TpmStatus::New();
  status->owned = is_owned;

  auto dictionary_attack = healthd::TpmDictionaryAttack::New();
  auto attestation = healthd::TpmAttestation::New();

  auto supported_features = healthd::TpmSupportedFeatures::New();
  supported_features->is_allowed = is_allowed;

  healthd::TpmInfoPtr tpm_info = healthd::TpmInfo::New();
  if (did_vid != "")
    tpm_info->did_vid = absl::optional<std::string>(did_vid);

  tpm_info->version = std::move(version);
  tpm_info->status = std::move(status);
  tpm_info->dictionary_attack = std::move(dictionary_attack);
  tpm_info->attestation = std::move(attestation);
  tpm_info->supported_features = std::move(supported_features);

  telemetry_info->tpm_result =
      healthd::TpmResult::NewTpmInfo(std::move(tpm_info));
}

void SetGraphicsInfo(healthd::TelemetryInfoPtr& telemetry_info,
                     std::vector<std::string>& gl_extensions) {
  auto gles_info = healthd::GLESInfo::New();
  gles_info->version = "fake_version";
  gles_info->shading_version = "fake_shading_version";
  gles_info->vendor = "fake_vendor";
  gles_info->renderer = "fake_renderer";
  gles_info->extensions = gl_extensions;

  healthd::GraphicsInfoPtr graphics_info = healthd::GraphicsInfo::New();
  graphics_info->gles_info = std::move(gles_info);

  auto egl_info = healthd::EGLInfo::New();
  graphics_info->egl_info = std::move(egl_info);

  telemetry_info->graphics_result =
      healthd::GraphicsResult::NewGraphicsInfo(std::move(graphics_info));
}

}  // namespace

class RevenLogSourceTest : public ::testing::Test {
 public:
  RevenLogSourceTest() {
    ash::cros_healthd::FakeCrosHealthd::Initialize();
    source_ = std::make_unique<RevenLogSource>();
  }

  ~RevenLogSourceTest() override {
    source_.reset();
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<SystemLogsResponse> Fetch() {
    std::unique_ptr<SystemLogsResponse> result;
    base::RunLoop run_loop;
    source_->Fetch(base::BindOnce(
        [](std::unique_ptr<SystemLogsResponse>* result,
           base::OnceClosure quit_closure,
           std::unique_ptr<SystemLogsResponse> response) {
          *result = std::move(response);
          std::move(quit_closure).Run();
        },
        &result, run_loop.QuitClosure()));
    run_loop.Run();

    return result;
  }

  void VerifyOutputContains(std::unique_ptr<SystemLogsResponse> response,
                            const std::string& expected_output) {
    ASSERT_NE(response, nullptr);
    const auto revenlog_iter = response->find(kRevenLogKey);
    ASSERT_NE(revenlog_iter, response->end());
    EXPECT_THAT(revenlog_iter->second, HasSubstr(expected_output));
  }

  void VerifyBiosBootMode(healthd::BootMode boot_mode,
                          const std::string& expected) {
    auto info = healthd::TelemetryInfo::New();
    auto os_info = CreateOsInfo(boot_mode);
    auto dmi_info = CreateDmiInfo();
    SetSystemInfo(info, std::move(os_info), std::move(dmi_info));
    ash::cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(info);

    VerifyOutputContains(Fetch(), expected);
  }

  void VerifyTpmInfo(uint32_t version,
                     const std::string& expected_version,
                     const std::string& did_vid,
                     const std::string& expected_did_vid,
                     bool is_owned,
                     bool is_allowed) {
    auto info = healthd::TelemetryInfo::New();
    SetTpmInfo(info, did_vid, version, is_owned, is_allowed);
    ash::cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(info);

    std::unique_ptr<SystemLogsResponse> response = Fetch();
    ASSERT_NE(response, nullptr);
    const auto revenlog_iter = response->find(kRevenLogKey);
    ASSERT_NE(revenlog_iter, response->end());

    EXPECT_THAT(revenlog_iter->second, HasSubstr("tpm_info:"));
    EXPECT_THAT(revenlog_iter->second, HasSubstr(expected_version));
    EXPECT_THAT(revenlog_iter->second, HasSubstr("\n  spec_level: 116"));
    EXPECT_THAT(revenlog_iter->second,
                HasSubstr("\n  manufacturer: 1129467731"));
    EXPECT_THAT(
        revenlog_iter->second,
        HasSubstr(is_owned ? "\n  tpm_owned: true" : "\n  tpm_owned: false"));
    EXPECT_THAT(revenlog_iter->second,
                HasSubstr(is_allowed ? "\n  tpm_allow_listed: true"
                                     : "\n  tpm_allow_listed: false"));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  std::unique_ptr<RevenLogSource> source_;
};

TEST_F(RevenLogSourceTest, FetchCpuInfoSuccess) {
  auto info = healthd::TelemetryInfo::New();
  SetCpuInfo(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(revenlog_iter->second, HasSubstr("cpuinfo:\n"));
  EXPECT_THAT(
      revenlog_iter->second,
      HasSubstr("\n  cpu_name: Intel(R) Core(TM) i5-10210U CPU @ 1.60GHz"));
}

TEST_F(RevenLogSourceTest, FetchCpuInfoFailure) {
  auto info = healthd::TelemetryInfo::New();
  SetCpuInfoWithProbeError(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_EQ(revenlog_iter->second.find(kCpuNameKey), std::string::npos);
}

TEST_F(RevenLogSourceTest, FetchMemoryInfoSuccess) {
  auto info = healthd::TelemetryInfo::New();
  SetMemoryInfo(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(meminfo:
  total_memory_kib: 2048
  free_memory_kib: 1024
  available_memory_kib: 512)";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, FetchMemoryInfoFailure) {
  auto info = healthd::TelemetryInfo::New();
  SetMemoryInfoWithProbeError(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_EQ(revenlog_iter->second.find(kTotalMemoryKey), std::string::npos);
  EXPECT_EQ(revenlog_iter->second.find(kFreeMemoryKey), std::string::npos);
  EXPECT_EQ(revenlog_iter->second.find(kAvailableMemoryKey), std::string::npos);
}

TEST_F(RevenLogSourceTest, FetchDmiInfoWithValues) {
  auto info = healthd::TelemetryInfo::New();
  auto os_info = CreateOsInfo(healthd::BootMode::kUnknown);
  auto dmi_info = CreateDmiInfo();
  SetSystemInfo(info, std::move(os_info), std::move(dmi_info));
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_vendor: LENOVO"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_name: 20U9001PUS"));
  EXPECT_THAT(revenlog_iter->second,
              HasSubstr("product_version: ThinkPad X1 Carbon Gen 8"));

  EXPECT_THAT(revenlog_iter->second, HasSubstr("bios_info:\n"));
  EXPECT_THAT(revenlog_iter->second,
              HasSubstr("\n  bios_version: N2WET26W (1.16 )"));
}

TEST_F(RevenLogSourceTest, FetchDmiInfoWithoutValues) {
  auto info = healthd::TelemetryInfo::New();
  auto os_info = CreateOsInfo(healthd::BootMode::kCrosEfi);
  auto dmi_info = healthd::DmiInfo::New();
  SetSystemInfo(info, std::move(os_info), std::move(dmi_info));
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_vendor: \n"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_name: \n"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("product_version: \n"));

  EXPECT_THAT(revenlog_iter->second, HasSubstr("bios_info:\n"));
  EXPECT_THAT(revenlog_iter->second, HasSubstr("\n  bios_version: \n"));
}

// BootMode::kCrosEfi: boot with EFI but not with secure boot
//  secureboot = false
//  uefi = true
TEST_F(RevenLogSourceTest, BiosBootMode_Uefi_True_SecureBoot_False) {
  std::string expected_output = R"(bios_info:
  bios_version: N2WET26W (1.16 )
  secureboot: false
  uefi: true)";
  VerifyBiosBootMode(healthd::BootMode::kCrosEfi, expected_output);
}

// BootMode::kCrosEfiSecure: boot with EFI security boot
//  secureboot = true
//  uefi = true
TEST_F(RevenLogSourceTest, BiosBootMode_Uefi_True_SecureBoot_True) {
  std::string expected_output = R"(bios_info:
  bios_version: N2WET26W (1.16 )
  secureboot: true
  uefi: true)";
  VerifyBiosBootMode(healthd::BootMode::kCrosEfiSecure, expected_output);
}

TEST_F(RevenLogSourceTest, BiosBootMode_SecureBoot_False_Uefi_False) {
  std::string expected_output = R"(bios_info:
  bios_version: N2WET26W (1.16 )
  secureboot: false
  uefi: false)";
  VerifyBiosBootMode(healthd::BootMode::kCrosSecure, expected_output);
  VerifyBiosBootMode(healthd::BootMode::kCrosLegacy, expected_output);
  VerifyBiosBootMode(healthd::BootMode::kUnknown, expected_output);
}

TEST_F(RevenLogSourceTest, PciEthernetDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetPciEthernetDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(ethernet_adapter_info:
  ethernet_adapter_name: intel product1
  ethernet_adapter_id: 12ab:34cd
  ethernet_adapter_bus: pci
  ethernet_adapter_driver: driver1

  ethernet_adapter_name: broadcom product2
  ethernet_adapter_id: 56ab:78cd
  ethernet_adapter_bus: pci
  ethernet_adapter_driver: )";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, PciBluetoothDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetPciBluetoothDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(bluetooth_adapter_info:
  bluetooth_adapter_name: intel bluetooth_product1
  bluetooth_adapter_id: 12ab:34cd
  bluetooth_adapter_bus: pci
  bluetooth_adapter_driver: bluetooth_driver1

  bluetooth_adapter_name: broadcom bluetooth_product2
  bluetooth_adapter_id: 56ab:78cd
  bluetooth_adapter_bus: pci
  bluetooth_adapter_driver: )";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, PciWirelessDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetPciWirelessDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(wireless_adapter_info:
  wireless_adapter_name: intel wireless_product1
  wireless_adapter_id: 12ab:34cd
  wireless_adapter_bus: pci
  wireless_adapter_driver: wireless_driver1

  wireless_adapter_name: broadcom wireless_product2
  wireless_adapter_id: 56ab:78cd
  wireless_adapter_bus: pci
  wireless_adapter_driver: )";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, PciGpuInfo) {
  auto info = healthd::TelemetryInfo::New();
  SetPciDisplayDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(gpu_info:
  gpu_name: intel 945GM
  gpu_id: 12ab:34cd
  gpu_bus: pci
  gpu_driver: driver1)";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, UsbEthernetDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetUsbEthernetDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(ethernet_adapter_info:
  ethernet_adapter_name: intel product1
  ethernet_adapter_id: 12ab:34cd
  ethernet_adapter_bus: usb
  ethernet_adapter_driver: driver1

  ethernet_adapter_name: broadcom product2
  ethernet_adapter_id: 56ab:78cd
  ethernet_adapter_bus: usb
  ethernet_adapter_driver: )";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, UsbWirelessDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetUsbWirelessDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(wireless_adapter_info:
  wireless_adapter_name: intel wireless_product1
  wireless_adapter_id: 12ab:34cd
  wireless_adapter_bus: usb
  wireless_adapter_driver: wireless_driver1

  wireless_adapter_name: broadcom wireless_product2
  wireless_adapter_id: 56ab:78cd
  wireless_adapter_bus: usb
  wireless_adapter_driver: )";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, UsbBluetoothDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetUsbBluetoothDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(bluetooth_adapter_info:
  bluetooth_adapter_name: intel bluetooth_product1
  bluetooth_adapter_id: 12ab:34cd
  bluetooth_adapter_bus: usb
  bluetooth_adapter_driver: bluetooth_driver1

  bluetooth_adapter_name: broadcom bluetooth_product2
  bluetooth_adapter_id: 56ab:78cd
  bluetooth_adapter_bus: usb
  bluetooth_adapter_driver: )";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, UsbGpuInfo) {
  auto info = healthd::TelemetryInfo::New();
  SetUsbDisplayDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(gpu_info:
  gpu_name: intel product1
  gpu_id: 12ab:34cd
  gpu_bus: usb
  gpu_driver: driver1)";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, TpmInfoVersion_1_2WithDidVid_Owned_Allowed) {
  VerifyTpmInfo(0x312e3200, "\n  tpm_version: 1.2", "286536196",
                "\n  did_vid: 286536196", true, true);
}

TEST_F(RevenLogSourceTest, TpmInfoVersion_2_0WithoutDidVid_Owned_NotAllowed) {
  VerifyTpmInfo(0x322e3000, "\n  tpm_version: 2.0", "", "\n  did_vid: \n", true,
                false);
}

TEST_F(RevenLogSourceTest,
       TpmInfoVersionUnknownWithoutDidVid_Allowed_NotOwned) {
  VerifyTpmInfo(0xaaaaaaaa, "\n  tpm_version: unknown", "", "\n  did_vid: \n",
                false, true);
}

TEST_F(RevenLogSourceTest,
       TpmInfoVersionUnknownWithoutDidVid_NotAllowed_NotOwned) {
  VerifyTpmInfo(0xaaaaaaaa, "\n  tpm_version: unknown", "", "\n  did_vid: \n",
                false, false);
}

TEST_F(RevenLogSourceTest, GraphicsInfoNoExtensions) {
  auto info = healthd::TelemetryInfo::New();
  std::vector<std::string> extensions;
  SetGraphicsInfo(info, extensions);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(graphics_info:
  gl_version: fake_version
  gl_shading_version: fake_shading_version
  gl_vendor: fake_vendor
  gl_renderer: fake_renderer
  gl_extensions: )";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, GraphicsInfoOneExtension) {
  auto info = healthd::TelemetryInfo::New();
  std::vector<std::string> extensions{"ext1"};
  SetGraphicsInfo(info, extensions);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(graphics_info:
  gl_version: fake_version
  gl_shading_version: fake_shading_version
  gl_vendor: fake_vendor
  gl_renderer: fake_renderer
  gl_extensions: ext1)";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, GraphicsInfoTwoExtensions) {
  auto info = healthd::TelemetryInfo::New();
  std::vector<std::string> extensions{"ext1", "ext2"};
  SetGraphicsInfo(info, extensions);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::string expected_output = R"(graphics_info:
  gl_version: fake_version
  gl_shading_version: fake_shading_version
  gl_vendor: fake_vendor
  gl_renderer: fake_renderer
  gl_extensions: ext1, ext2)";
  VerifyOutputContains(Fetch(), expected_output);
}

TEST_F(RevenLogSourceTest, TouchpadStack) {
  auto info = healthd::TelemetryInfo::New();
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto revenlog_iter = response->find(kRevenLogKey);
  ASSERT_NE(revenlog_iter, response->end());

  EXPECT_THAT(
      revenlog_iter->second,
      AnyOf(HasSubstr("touchpad_stack: libinput\n"),
            HasSubstr("touchpad_stack: gestures\n"),
            HasSubstr("touchpad_stack: Default EventConverterEvdev\n")));
}

}  // namespace system_logs
