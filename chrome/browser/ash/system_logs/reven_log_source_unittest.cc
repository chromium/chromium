// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/reven_log_source.h"

#include <optional>

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

namespace system_logs {

namespace {

namespace healthd = ::ash::cros_healthd::mojom;
using ::testing::HasSubstr;

constexpr char kRevenAvailableMemoryKey[] = "chromeosflex_available_memory";
constexpr char kRevenBiosVersionKey[] = "chromeosflex_bios_version";
constexpr char kRevenBluetoothDriverKey[] = "chromeosflex_bluetooth_driver";
constexpr char kRevenBluetoothIdKey[] = "chromeosflex_bluetooth_id";
constexpr char kRevenBluetoothNameKey[] = "chromeosflex_bluetooth_name";
constexpr char kRevenCpuNameKey[] = "chromeosflex_cpu_name";
constexpr char kRevenEthernetDriverKey[] = "chromeosflex_ethernet_driver";
constexpr char kRevenEthernetIdKey[] = "chromeosflex_ethernet_id";
constexpr char kRevenEthernetNameKey[] = "chromeosflex_ethernet_name";
constexpr char kRevenFreeMemoryKey[] = "chromeosflex_free_memory";
constexpr char kRevenGlExtensionsKey[] = "chromeosflex_gl_extensions";
constexpr char kRevenGlRendererKey[] = "chromeosflex_gl_renderer";
constexpr char kRevenGlShadingVersionKey[] = "chromeosflex_gl_shading_version";
constexpr char kRevenGlVendorKey[] = "chromeosflex_gl_vendor";
constexpr char kRevenGlVersionKey[] = "chromeosflex_gl_version";
constexpr char kRevenGpuDriverKey[] = "chromeosflex_gpu_driver";
constexpr char kRevenGpuIdKey[] = "chromeosflex_gpu_id";
constexpr char kRevenGpuNameKey[] = "chromeosflex_gpu_name";
constexpr char kRevenProductNameKey[] = "chromeosflex_product_name";
constexpr char kRevenProductVendorKey[] = "chromeosflex_product_vendor";
constexpr char kRevenProductVersionKey[] = "chromeosflex_product_version";
constexpr char kRevenSecurebootKey[] = "chromeosflex_secureboot";
constexpr char kRevenTotalMemoryKey[] = "chromeosflex_total_memory";
constexpr char kRevenTouchpadStack[] = "chromeosflex_touchpad";
constexpr char kRevenTpmAllowListedKey[] = "chromeosflex_tpm_allow_listed";
constexpr char kRevenTpmDidVidKey[] = "chromeosflex_tpm_did_vid";
constexpr char kRevenTpmManufacturerKey[] = "chromeosflex_tpm_manufacturer";
constexpr char kRevenTpmOwnedKey[] = "chromeosflex_tpm_owned";
constexpr char kRevenTpmSpecLevelKey[] = "chromeosflex_tpm_spec_level";
constexpr char kRevenTpmVersionKey[] = "chromeosflex_tpm_version";
constexpr char kRevenUefiKey[] = "chromeosflex_uefi";
constexpr char kRevenWirelessDriverKey[] = "chromeosflex_wireless_driver";
constexpr char kRevenWirelessIdKey[] = "chromeosflex_wireless_id";
constexpr char kRevenWirelessNameKey[] = "chromeosflex_wireless_name";

constexpr char kCpuNameVal[] = "Intel(R) Core(TM) i5-10210U CPU @ 1.60GHz";

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
  dmi_info->sys_vendor = std::optional<std::string>("LENOVO");
  dmi_info->product_name = std::optional<std::string>("20U9001PUS");
  dmi_info->product_version =
      std::optional<std::string>("ThinkPad X1 Carbon Gen 8");
  dmi_info->bios_version = std::optional<std::string>("N2WET26W (1.16 )");
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
  if (driver != "") {
    pci_info->driver = std::optional<std::string>(driver);
  }

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
  if (driver != "") {
    usb_if_info->driver = std::optional<std::string>(driver);
  }
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
  if (did_vid != "") {
    tpm_info->did_vid = std::optional<std::string>(did_vid);
  }

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

  void SetBootModeInfo(healthd::BootMode boot_mode) {
    auto info = healthd::TelemetryInfo::New();
    auto os_info = CreateOsInfo(boot_mode);
    auto dmi_info = CreateDmiInfo();
    SetSystemInfo(info, std::move(os_info), std::move(dmi_info));
    ash::cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(info);
  }

  void VerifyOutputContains(const std::unique_ptr<SystemLogsResponse>& response,
                            const std::string& key,
                            const std::string& expected_output) {
    ASSERT_NE(response, nullptr);
    const auto response_iter = response->find(key);
    ASSERT_NE(response_iter, response->end());
    EXPECT_THAT(response_iter->second, HasSubstr(expected_output));
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

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenCpuNameKey,
                       "Intel(R) Core(TM) i5-10210U CPU @ 1.60GHz");
}

TEST_F(RevenLogSourceTest, FetchCpuInfoFailure) {
  auto info = healthd::TelemetryInfo::New();
  SetCpuInfoWithProbeError(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  VerifyOutputContains(Fetch(), kRevenCpuNameKey, "<not available>");
}

TEST_F(RevenLogSourceTest, FetchMemoryInfoSuccess) {
  auto info = healthd::TelemetryInfo::New();
  SetMemoryInfo(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenTotalMemoryKey, "2048");
  VerifyOutputContains(response, kRevenFreeMemoryKey, "1024");
  VerifyOutputContains(response, kRevenAvailableMemoryKey, "512");
}

TEST_F(RevenLogSourceTest, FetchMemoryInfoFailure) {
  auto info = healthd::TelemetryInfo::New();
  SetMemoryInfoWithProbeError(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  const std::string not_available = "<not available>";
  VerifyOutputContains(response, kRevenTotalMemoryKey, not_available);
  VerifyOutputContains(response, kRevenFreeMemoryKey, not_available);
  VerifyOutputContains(response, kRevenAvailableMemoryKey, not_available);
}

TEST_F(RevenLogSourceTest, FetchDmiInfoWithValues) {
  auto info = healthd::TelemetryInfo::New();
  auto os_info = CreateOsInfo(healthd::BootMode::kUnknown);
  auto dmi_info = CreateDmiInfo();
  SetSystemInfo(info, std::move(os_info), std::move(dmi_info));
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenProductVendorKey, "LENOVO");
  VerifyOutputContains(response, kRevenProductNameKey, "20U9001PUS");
  VerifyOutputContains(response, kRevenProductVersionKey,
                       "ThinkPad X1 Carbon Gen 8");
  VerifyOutputContains(response, kRevenBiosVersionKey, "N2WET26W (1.16 )");
}

TEST_F(RevenLogSourceTest, FetchDmiInfoWithoutValues) {
  auto info = healthd::TelemetryInfo::New();
  auto os_info = CreateOsInfo(healthd::BootMode::kCrosEfi);
  auto dmi_info = healthd::DmiInfo::New();
  SetSystemInfo(info, std::move(os_info), std::move(dmi_info));
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  const std::string not_available = "<not available>";
  VerifyOutputContains(response, kRevenProductVendorKey, not_available);
  VerifyOutputContains(response, kRevenProductNameKey, not_available);
  VerifyOutputContains(response, kRevenProductVersionKey, not_available);
  VerifyOutputContains(response, kRevenBiosVersionKey, not_available);
}

// BootMode::kCrosEfi: boot with EFI but not with secure boot.
TEST_F(RevenLogSourceTest, BiosBootMode_Uefi_True_SecureBoot_False) {
  SetBootModeInfo(healthd::BootMode::kCrosEfi);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenSecurebootKey, "false");
  VerifyOutputContains(response, kRevenUefiKey, "true");
}

// BootMode::kCrosEfiSecure: boot with EFI security boot.
TEST_F(RevenLogSourceTest, BiosBootMode_Uefi_True_SecureBoot_True) {
  SetBootModeInfo(healthd::BootMode::kCrosEfiSecure);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenSecurebootKey, "true");
  VerifyOutputContains(response, kRevenUefiKey, "true");
}

// BootMode::kCrosSecure: Chromebook/box firmware.
TEST_F(RevenLogSourceTest, BiosBootMode_CrosSecure) {
  SetBootModeInfo(healthd::BootMode::kCrosSecure);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenSecurebootKey, "false");
  VerifyOutputContains(response, kRevenUefiKey, "false");
}

// BootMode::kCrosLegacy: Old BIOS firmware, or UEFI in "compatibility mode".
TEST_F(RevenLogSourceTest, BiosBootMode_CrosLegacy) {
  SetBootModeInfo(healthd::BootMode::kCrosLegacy);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenSecurebootKey, "false");
  VerifyOutputContains(response, kRevenUefiKey, "false");
}

// BootMode::kUnknown: An issue with detection.
TEST_F(RevenLogSourceTest, BiosBootMode_Unknown) {
  SetBootModeInfo(healthd::BootMode::kUnknown);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenSecurebootKey, "false");
  VerifyOutputContains(response, kRevenUefiKey, "false");
}

TEST_F(RevenLogSourceTest, PciEthernetDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetPciEthernetDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenEthernetNameKey, "intel product1");
  VerifyOutputContains(response, kRevenEthernetNameKey, "broadcom product2");
  VerifyOutputContains(response, kRevenEthernetIdKey, "pci:12ab:34cd");
  VerifyOutputContains(response, kRevenEthernetIdKey, "pci:56ab:78cd");
  VerifyOutputContains(response, kRevenEthernetDriverKey, "driver1");
}

TEST_F(RevenLogSourceTest, PciBluetoothDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetPciBluetoothDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenBluetoothNameKey,
                       "intel bluetooth_product1");
  VerifyOutputContains(response, kRevenBluetoothNameKey,
                       "broadcom bluetooth_product2");
  VerifyOutputContains(response, kRevenBluetoothIdKey, "pci:12ab:34cd");
  VerifyOutputContains(response, kRevenBluetoothIdKey, "pci:56ab:78cd");
  VerifyOutputContains(response, kRevenBluetoothDriverKey, "bluetooth_driver1");
}

TEST_F(RevenLogSourceTest, PciWirelessDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetPciWirelessDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenWirelessNameKey,
                       "intel wireless_product1");
  VerifyOutputContains(response, kRevenWirelessNameKey,
                       "broadcom wireless_product2");
  VerifyOutputContains(response, kRevenWirelessIdKey, "pci:12ab:34cd");
  VerifyOutputContains(response, kRevenWirelessIdKey, "pci:56ab:78cd");
  VerifyOutputContains(response, kRevenWirelessDriverKey, "wireless_driver1");
}

TEST_F(RevenLogSourceTest, PciGpuInfo) {
  auto info = healthd::TelemetryInfo::New();
  SetPciDisplayDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenGpuNameKey, "intel 945GM");
  VerifyOutputContains(response, kRevenGpuIdKey, "pci:12ab:34cd");
  VerifyOutputContains(response, kRevenGpuDriverKey, "driver1");
}

TEST_F(RevenLogSourceTest, UsbEthernetDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetUsbEthernetDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenEthernetNameKey, "intel product1");
  VerifyOutputContains(response, kRevenEthernetNameKey, "broadcom product2");
  VerifyOutputContains(response, kRevenEthernetIdKey, "usb:12ab:34cd");
  VerifyOutputContains(response, kRevenEthernetIdKey, "usb:56ab:78cd");
  VerifyOutputContains(response, kRevenEthernetDriverKey, "driver1");
}

TEST_F(RevenLogSourceTest, UsbBluetoothDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetUsbBluetoothDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenBluetoothNameKey,
                       "intel bluetooth_product1");
  VerifyOutputContains(response, kRevenBluetoothNameKey,
                       "broadcom bluetooth_product2");
  VerifyOutputContains(response, kRevenBluetoothIdKey, "usb:12ab:34cd");
  VerifyOutputContains(response, kRevenBluetoothIdKey, "usb:56ab:78cd");
  VerifyOutputContains(response, kRevenBluetoothDriverKey, "bluetooth_driver1");
}

TEST_F(RevenLogSourceTest, UsbWirelessDevices) {
  auto info = healthd::TelemetryInfo::New();
  SetUsbWirelessDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenWirelessNameKey,
                       "intel wireless_product1");
  VerifyOutputContains(response, kRevenWirelessNameKey,
                       "broadcom wireless_product2");
  VerifyOutputContains(response, kRevenWirelessIdKey, "usb:12ab:34cd");
  VerifyOutputContains(response, kRevenWirelessIdKey, "usb:56ab:78cd");
  VerifyOutputContains(response, kRevenWirelessDriverKey, "wireless_driver1");
}

TEST_F(RevenLogSourceTest, UsbGpuInfo) {
  auto info = healthd::TelemetryInfo::New();
  SetUsbDisplayDevices(info);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenGpuNameKey, "intel product1");
  VerifyOutputContains(response, kRevenGpuIdKey, "usb:12ab:34cd");
  VerifyOutputContains(response, kRevenGpuDriverKey, "driver1");
}

TEST_F(RevenLogSourceTest, TpmInfoVersion_1_2WithDidVid_Owned_Allowed) {
  const uint32_t version = 0x312e3200;  // TPM 1.2
  const std::string did_vid = "286536196";
  const bool is_owned = true;
  const bool is_allowed = true;

  auto info = healthd::TelemetryInfo::New();
  SetTpmInfo(info, did_vid, version, is_owned, is_allowed);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenTpmVersionKey, "1.2");
  VerifyOutputContains(response, kRevenTpmSpecLevelKey, "116");
  VerifyOutputContains(response, kRevenTpmManufacturerKey, "1129467731");
  VerifyOutputContains(response, kRevenTpmDidVidKey, "286536196");
  VerifyOutputContains(response, kRevenTpmOwnedKey, "true");
  VerifyOutputContains(response, kRevenTpmAllowListedKey, "true");
}

TEST_F(RevenLogSourceTest, TpmInfoVersion_2_0WithoutDidVid_Owned_NotAllowed) {
  const uint32_t version = 0x322e3000;  // TPM 2.0
  const std::string did_vid = "";
  const bool is_owned = true;
  const bool is_allowed = false;

  auto info = healthd::TelemetryInfo::New();
  SetTpmInfo(info, did_vid, version, is_owned, is_allowed);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenTpmVersionKey, "2.0");
  VerifyOutputContains(response, kRevenTpmSpecLevelKey, "116");
  VerifyOutputContains(response, kRevenTpmManufacturerKey, "1129467731");
  VerifyOutputContains(response, kRevenTpmDidVidKey, "");
  VerifyOutputContains(response, kRevenTpmOwnedKey, "true");
  VerifyOutputContains(response, kRevenTpmAllowListedKey, "false");
}

TEST_F(RevenLogSourceTest,
       TpmInfoVersionUnknownWithoutDidVid_Allowed_NotOwned) {
  const uint32_t version = 0xaaaaaaaa;
  const std::string did_vid = "";
  const bool is_owned = false;
  const bool is_allowed = true;

  auto info = healthd::TelemetryInfo::New();
  SetTpmInfo(info, did_vid, version, is_owned, is_allowed);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenTpmVersionKey, "unknown");
  VerifyOutputContains(response, kRevenTpmSpecLevelKey, "116");
  VerifyOutputContains(response, kRevenTpmManufacturerKey, "1129467731");
  VerifyOutputContains(response, kRevenTpmDidVidKey, "");
  VerifyOutputContains(response, kRevenTpmOwnedKey, "false");
  VerifyOutputContains(response, kRevenTpmAllowListedKey, "true");
}

TEST_F(RevenLogSourceTest,
       TpmInfoVersionUnknownWithoutDidVid_NotAllowed_NotOwned) {
  const uint32_t version = 0xaaaaaaaa;
  const std::string did_vid = "";
  const bool is_owned = false;
  const bool is_allowed = false;

  auto info = healthd::TelemetryInfo::New();
  SetTpmInfo(info, did_vid, version, is_owned, is_allowed);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenTpmVersionKey, "unknown");
  VerifyOutputContains(response, kRevenTpmSpecLevelKey, "116");
  VerifyOutputContains(response, kRevenTpmManufacturerKey, "1129467731");
  VerifyOutputContains(response, kRevenTpmDidVidKey, "");
  VerifyOutputContains(response, kRevenTpmOwnedKey, "false");
  VerifyOutputContains(response, kRevenTpmAllowListedKey, "false");
}

TEST_F(RevenLogSourceTest, GraphicsInfoNoExtensions) {
  auto info = healthd::TelemetryInfo::New();
  std::vector<std::string> extensions;
  SetGraphicsInfo(info, extensions);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenGlVersionKey, "fake_version");
  VerifyOutputContains(response, kRevenGlShadingVersionKey,
                       "fake_shading_version");
  VerifyOutputContains(response, kRevenGlVendorKey, "fake_vendor");
  VerifyOutputContains(response, kRevenGlRendererKey, "fake_renderer");
  VerifyOutputContains(response, kRevenGlExtensionsKey, "");
}

TEST_F(RevenLogSourceTest, GraphicsInfoOneExtension) {
  auto info = healthd::TelemetryInfo::New();
  std::vector<std::string> extensions{"ext1"};
  SetGraphicsInfo(info, extensions);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenGlVersionKey, "fake_version");
  VerifyOutputContains(response, kRevenGlShadingVersionKey,
                       "fake_shading_version");
  VerifyOutputContains(response, kRevenGlVendorKey, "fake_vendor");
  VerifyOutputContains(response, kRevenGlRendererKey, "fake_renderer");
  VerifyOutputContains(response, kRevenGlExtensionsKey, "ext1");
}

TEST_F(RevenLogSourceTest, GraphicsInfoTwoExtensions) {
  auto info = healthd::TelemetryInfo::New();
  std::vector<std::string> extensions{"ext1", "ext2"};
  SetGraphicsInfo(info, extensions);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  VerifyOutputContains(response, kRevenGlVersionKey, "fake_version");
  VerifyOutputContains(response, kRevenGlShadingVersionKey,
                       "fake_shading_version");
  VerifyOutputContains(response, kRevenGlVendorKey, "fake_vendor");
  VerifyOutputContains(response, kRevenGlRendererKey, "fake_renderer");
  VerifyOutputContains(response, kRevenGlExtensionsKey, "ext1, ext2");
}

TEST_F(RevenLogSourceTest, TouchpadStack) {
  auto info = healthd::TelemetryInfo::New();
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  ASSERT_NE(response, nullptr);
  const auto response_iter = response->find(kRevenTouchpadStack);
  ASSERT_NE(response_iter, response->end());

  EXPECT_THAT(response_iter->second,
              AnyOf(HasSubstr("libinput"), HasSubstr("gestures"),
                    HasSubstr("Default EventConverterEvdev")));
}

TEST_F(RevenLogSourceTest, NothingAvailable) {
  auto info = healthd::TelemetryInfo::New();

  const std::unique_ptr<SystemLogsResponse> response = Fetch();
  const std::string not_available = "<not available>";
  VerifyOutputContains(response, kRevenProductVendorKey, not_available);
  VerifyOutputContains(response, kRevenProductNameKey, not_available);
  VerifyOutputContains(response, kRevenProductVersionKey, not_available);
  VerifyOutputContains(response, kRevenBiosVersionKey, not_available);
  VerifyOutputContains(response, kRevenEthernetNameKey, not_available);
  VerifyOutputContains(response, kRevenEthernetIdKey, not_available);
  VerifyOutputContains(response, kRevenEthernetDriverKey, not_available);
  VerifyOutputContains(response, kRevenWirelessNameKey, not_available);
  VerifyOutputContains(response, kRevenWirelessIdKey, not_available);
  VerifyOutputContains(response, kRevenWirelessDriverKey, not_available);
  VerifyOutputContains(response, kRevenBluetoothNameKey, not_available);
  VerifyOutputContains(response, kRevenBluetoothIdKey, not_available);
  VerifyOutputContains(response, kRevenBluetoothDriverKey, not_available);
  VerifyOutputContains(response, kRevenGpuNameKey, not_available);
  VerifyOutputContains(response, kRevenGpuIdKey, not_available);
  VerifyOutputContains(response, kRevenGpuDriverKey, not_available);
  VerifyOutputContains(response, kRevenCpuNameKey, not_available);
  VerifyOutputContains(response, kRevenTotalMemoryKey, not_available);
  VerifyOutputContains(response, kRevenFreeMemoryKey, not_available);
  VerifyOutputContains(response, kRevenAvailableMemoryKey, not_available);
  VerifyOutputContains(response, kRevenSecurebootKey, not_available);
  VerifyOutputContains(response, kRevenUefiKey, not_available);
  VerifyOutputContains(response, kRevenTpmVersionKey, not_available);
  VerifyOutputContains(response, kRevenTpmSpecLevelKey, not_available);
  VerifyOutputContains(response, kRevenTpmManufacturerKey, not_available);
  VerifyOutputContains(response, kRevenTpmDidVidKey, not_available);
  VerifyOutputContains(response, kRevenTpmAllowListedKey, not_available);
  VerifyOutputContains(response, kRevenTpmOwnedKey, not_available);
  VerifyOutputContains(response, kRevenGlVersionKey, not_available);
  VerifyOutputContains(response, kRevenGlShadingVersionKey, not_available);
  VerifyOutputContains(response, kRevenGlVendorKey, not_available);
  VerifyOutputContains(response, kRevenGlRendererKey, not_available);
  VerifyOutputContains(response, kRevenGlExtensionsKey, not_available);
  // Touchpad stack is always set to something.
  VerifyOutputContains(response, kRevenTouchpadStack, "");
  // To remind folks to add all new keys here, also check total number of keys.
  uint16_t expected_num_keys = 34;
  EXPECT_THAT(response->size(), expected_num_keys);
}

}  // namespace system_logs
