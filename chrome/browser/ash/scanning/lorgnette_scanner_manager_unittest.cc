// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector_utils.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/fake_lorgnette_manager_client.h"
#include "chromeos/ash/components/dbus/lorgnette_manager/lorgnette_manager_client.h"
#include "chromeos/ash/components/scanning/scanner.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

using local_discovery::ServiceDescription;
using ::testing::ElementsAreArray;

// Test device names for different types of lorgnette scanners.
constexpr char kLorgnetteNetworkIpDeviceName[] = "test:MX3100_192.168.0.3";
constexpr char kLorgnetteNetworkUrlDeviceName[] =
    "http://testscanner.domain.org";
constexpr char kLorgnetteUsbDeviceName[] = "test:04A91752_94370B";
constexpr char kLorgnetteNetworkEpsonDeviceName[] = "epson2:net:192.168.0.3";

// A scanner name that does not correspond to a known scanner.
constexpr char kUnknownScannerName[] = "Unknown Scanner";

// Model which contains the manufacturer.
constexpr char kModelContainingManufacturer[] = "TEST Model X";

// Returns a ScannerInfo object with the given |name| and |model|, if provided.
lorgnette::ScannerInfo CreateLorgnetteScanner(
    std::string name,
    const std::string& model = "MX3100") {
  lorgnette::ScannerInfo scanner;
  scanner.set_name(name);
  scanner.set_manufacturer("Test");
  scanner.set_model(model);
  scanner.set_type("Flatbed");
  return scanner;
}

// Returns a ListScannersResponse containing a single ScannerInfo object created
// with the given |name| and |model|, if provided.
lorgnette::ListScannersResponse CreateListScannersResponse(
    std::string name,
    const std::string& model = "MX3100") {
  lorgnette::ScannerInfo scanner = CreateLorgnetteScanner(name, model);
  lorgnette::ListScannersResponse response;
  *response.add_scanners() = std::move(scanner);
  return response;
}

// Returns a zeroconf Scanner with the device name marked as |usable|.
Scanner CreateZeroconfScanner(bool usable = true) {
  return CreateSaneScanner("Test MX3100",
                           ZeroconfScannerDetector::kEsclsServiceType, "",
                           net::IPAddress(192, 168, 0, 3), 5, usable)
      .value();
}

// Returns a zeroconf Scanner with the Epsonds backend and the device name
// marked as |usable|
Scanner CreateNonEsclEpsonZeroconfScanner(bool usable = true) {
  return CreateSaneScanner("EPSON TEST",
                           ZeroconfScannerDetector::kGenericScannerServiceType,
                           "", net::IPAddress(192, 168, 0, 3), 5, usable)
      .value();
}

// Returns a zeroconf Scanner with an Epson name but ESCLs Service marked as
// |usable|.
Scanner CreateEsclEpsonZeroconfScanner(bool usable = true) {
  return CreateSaneScanner("EPSON TEST",
                           ZeroconfScannerDetector::kEsclsServiceType, "",
                           net::IPAddress(192, 168, 0, 3), 5, usable)
      .value();
}

// Returns a zeroconf Scanner with |scanner_name| and device name marked
// |usable|.
Scanner CreateScannerCustomName(const std::string& scanner_name,
                                bool usable = true) {
  return CreateSaneScanner(scanner_name,
                           ZeroconfScannerDetector::kEsclsServiceType, "",
                           net::IPAddress(192, 168, 0, 3), 5, usable)
      .value();
}

class FakeZeroconfScannerDetector final : public ZeroconfScannerDetector {
 public:
  FakeZeroconfScannerDetector() = default;
  ~FakeZeroconfScannerDetector() override = default;

  void RegisterScannersDetectedCallback(
      OnScannersDetectedCallback callback) override {
    on_scanners_detected_callback_ = std::move(callback);
  }

  std::vector<Scanner> GetScanners() override {
    std::vector<Scanner> scanners;
    for (const auto& entry : scanners_)
      scanners.push_back(entry.second);

    return scanners;
  }

  void OnDeviceChanged(const std::string& service_type,
                       bool added,
                       const ServiceDescription& service_description) override {
  }

  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override {}

  void OnDeviceCacheFlushed(const std::string& service_type) override {}

  // Used to trigger on_scanners_detected_callback_ after adding the given
  // |scanners| to the detected scanners.
  void AddDetections(const std::vector<Scanner>& scanners) {
    for (const auto& scanner : scanners)
      scanners_[scanner.display_name] = scanner;

    on_scanners_detected_callback_.Run(GetScanners());
  }

  // Used to trigger on_scanners_detected_callback_ after removing the given
  // |scanners| from the detected scanners.
  void RemoveDetections(const std::vector<Scanner>& scanners) {
    for (const auto& scanner : scanners)
      scanners_.erase(scanner.display_name);

    on_scanners_detected_callback_.Run(GetScanners());
  }

 private:
  base::flat_map<std::string, Scanner> scanners_;
  OnScannersDetectedCallback on_scanners_detected_callback_;
};

}  // namespace

class LorgnetteScannerManagerTest : public testing::Test {
 public:
  LorgnetteScannerManagerTest() {
    run_loop_ = std::make_unique<base::RunLoop>();
    LorgnetteManagerClient::InitializeFake();
    auto fake_zeroconf_scanner_detector =
        std::make_unique<FakeZeroconfScannerDetector>();
    fake_zeroconf_scanner_detector_ = fake_zeroconf_scanner_detector.get();
    lorgnette_scanner_manager_ = LorgnetteScannerManager::Create(
        std::move(fake_zeroconf_scanner_detector));
    // Set empty but successful capabilities response by default.
    lorgnette::ScannerCapabilities capabilities;
    GetLorgnetteManagerClient()->SetScannerCapabilitiesResponse(capabilities);
  }

  ~LorgnetteScannerManagerTest() override {
    LorgnetteManagerClient::Shutdown();
  }

  // Returns a FakeLorgnetteManagerClient with an empty but successful
  // GetCapabilities response by default.
  FakeLorgnetteManagerClient* GetLorgnetteManagerClient() {
    return static_cast<FakeLorgnetteManagerClient*>(
        LorgnetteManagerClient::Get());
  }

  // Calls LorgnetteScannerManager::GetScannerNames() and binds a callback to
  // process the result.
  void GetScannerNames() {
    lorgnette_scanner_manager_->GetScannerNames(
        base::BindOnce(&LorgnetteScannerManagerTest::GetScannerNamesCallback,
                       base::Unretained(this)));
  }

  // Calls LorgnetteScannerManager::GetScannerCapabilities() and binds a
  // callback to process the result.
  void GetScannerCapabilities(const std::string& scanner_name) {
    lorgnette_scanner_manager_->GetScannerCapabilities(
        scanner_name,
        base::BindOnce(
            &LorgnetteScannerManagerTest::GetScannerCapabilitiesCallback,
            base::Unretained(this)));
  }

  // Calls LorgnetteScannerManager::IsRotateAlternate() and returns result.
  bool GetRotateAlternate(const std::string& scanner_name,
                          const std::string& source_name) {
    return lorgnette_scanner_manager_->IsRotateAlternate(scanner_name,
                                                         source_name);
  }

  // Calls LorgnetteScannerManager::Scan() and binds a callback to process the
  // result.
  void Scan(const std::string& scanner_name,
            const lorgnette::ScanSettings& settings) {
    lorgnette_scanner_manager_->Scan(
        scanner_name, settings, base::NullCallback(),
        base::BindRepeating(&LorgnetteScannerManagerTest::PageCallback,
                            base::Unretained(this)),
        base::BindOnce(&LorgnetteScannerManagerTest::ScanCallback,
                       base::Unretained(this)));
  }

  // Calls LorgnetteScannerManager::CancelScan() and binds a callback to process
  // the result.
  void CancelScan() {
    lorgnette_scanner_manager_->CancelScan(
        base::BindOnce(&LorgnetteScannerManagerTest::CancelScanCallback,
                       base::Unretained(this)));
  }

  // Runs all tasks until the ThreadPool's non-delayed queues are empty.
  void CompleteTasks() { task_environment_.RunUntilIdle(); }

  // Runs run_loop_ until a callback calls Quit().
  void WaitForResult() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  FakeZeroconfScannerDetector* fake_zeroconf_scanner_detector() {
    return fake_zeroconf_scanner_detector_;
  }

  const std::vector<std::string>& scanner_names() const {
    return scanner_names_;
  }

  absl::optional<lorgnette::ScannerCapabilities> scanner_capabilities() const {
    return scanner_capabilities_;
  }

  std::vector<std::string> scan_data() const { return scan_data_; }
  lorgnette::ScanFailureMode failure_mode() const { return failure_mode_; }
  bool cancel_scan_success() const { return cancel_scan_success_; }

 private:
  // Handles the result of calling LorgnetteScannerManager::GetScannerNames().
  void GetScannerNamesCallback(std::vector<std::string> scanner_names) {
    scanner_names_ = scanner_names;
    run_loop_->Quit();
  }

  void GetScannerCapabilitiesCallback(
      const absl::optional<lorgnette::ScannerCapabilities>&
          scanner_capabilities) {
    scanner_capabilities_ = scanner_capabilities;
    run_loop_->Quit();
  }

  // Handles receiving a page from LorgnetteScannerManager::Scan().
  void PageCallback(std::string page_data, uint32_t /*page_number*/) {
    scan_data_.push_back(page_data);
  }

  // Handles completion of LorgnetteScannerManager::Scan().
  void ScanCallback(lorgnette::ScanFailureMode failure_mode) {
    failure_mode_ = failure_mode;
    run_loop_->Quit();
  }

  // Handles completion of LorgnetteScannerManager::CancelScan().
  void CancelScanCallback(bool success) {
    cancel_scan_success_ = success;
    run_loop_->Quit();
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<base::RunLoop> run_loop_;

  raw_ptr<FakeZeroconfScannerDetector, ExperimentalAsh>
      fake_zeroconf_scanner_detector_;

  std::unique_ptr<LorgnetteScannerManager> lorgnette_scanner_manager_;

  std::vector<std::string> scanner_names_;
  absl::optional<lorgnette::ScannerCapabilities> scanner_capabilities_;
  lorgnette::ScanFailureMode failure_mode_ =
      lorgnette::SCAN_FAILURE_MODE_NO_FAILURE;
  bool cancel_scan_success_ = false;
  std::vector<std::string> scan_data_;
};

// Test that no scanner names are returned when no scanners have been detected.
TEST_F(LorgnetteScannerManagerTest, NoScanners) {
  GetScannerNames();
  WaitForResult();
  EXPECT_TRUE(scanner_names().empty());
}

// Test that no scanner names are returned when scanners are detected, but none
// return capabilities.
TEST_F(LorgnetteScannerManagerTest, NoScannersWithNoCap) {
  GetLorgnetteManagerClient()->SetListScannersResponse(
      CreateListScannersResponse(kLorgnetteNetworkIpDeviceName));
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  auto epson_scanner = CreateNonEsclEpsonZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({epson_scanner});
  GetLorgnetteManagerClient()->SetScannerCapabilitiesResponse(absl::nullopt);
  GetScannerNames();
  WaitForResult();
  EXPECT_TRUE(scanner_names().empty());
}

// Test that the name of a detected zeroconf scanner can be retrieved.
TEST_F(LorgnetteScannerManagerTest, ZeroconfScanner) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner.display_name}));
}

// Test that the name of a detected Epson zeroconf scanner can be retrieved.
TEST_F(LorgnetteScannerManagerTest, NonEsclEpsonZeroconfScanner) {
  auto scanner = CreateNonEsclEpsonZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner.display_name}));
}

// Test that the name of a detected ESCL Epson zeroconf scanner can be
// retrieved.
TEST_F(LorgnetteScannerManagerTest, EsclEpsonZeroconfScanner) {
  auto scanner = CreateEsclEpsonZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner.display_name}));
}

// Test that the name of a detected non-ESCL zeroconf scanner does not generate
// a scanner if it is not an Epson.
TEST_F(LorgnetteScannerManagerTest, NonEsclNonEpsonZeroconfScanner) {
  absl::optional<Scanner> scanner = CreateSaneScanner(
      "Test MX3100", ZeroconfScannerDetector::kGenericScannerServiceType, "",
      net::IPAddress(192, 168, 0, 3), 5, true);
  EXPECT_FALSE(scanner.has_value());
}

// Test that the name of a detected lorgnette scanner can be retrieved.
TEST_F(LorgnetteScannerManagerTest, LorgnetteScanner) {
  lorgnette::ListScannersResponse response =
      CreateListScannersResponse(kLorgnetteNetworkIpDeviceName);
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  GetScannerNames();
  WaitForResult();
  const auto& scanner = response.scanners()[0];
  std::string scanner_name = scanner.manufacturer() + " " + scanner.model();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner_name}));
}

// Test that two detected scanners with the same IP address are deduplicated and
// reported with single scanner name.
TEST_F(LorgnetteScannerManagerTest, DeduplicateScanner) {
  GetLorgnetteManagerClient()->SetListScannersResponse(
      CreateListScannersResponse(kLorgnetteNetworkIpDeviceName));
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  auto epson_scanner = CreateNonEsclEpsonZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({epson_scanner});
  auto escl_epson_scanner = CreateEsclEpsonZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({escl_epson_scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(), ElementsAreArray({epson_scanner.display_name,
                                                 scanner.display_name}));
}

// Test that two detected zeroconf Epson scanners and a lorgnette Epson scanner
// with the same IP address are deduplicated and reported with single scanner
// name.
TEST_F(LorgnetteScannerManagerTest, DeduplicateLorgnetteEpsonScanner) {
  GetLorgnetteManagerClient()->SetListScannersResponse(
      CreateListScannersResponse(kLorgnetteNetworkEpsonDeviceName,
                                 "EPSON Test2"));
  auto epson_scanner = CreateNonEsclEpsonZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({epson_scanner});
  auto escl_epson_scanner = CreateEsclEpsonZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({escl_epson_scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(), ElementsAreArray({epson_scanner.display_name}));
}

// Test that two detected scanners with the same IP address are deduplicated and
// reported with single scanner name, while USB of the same model is reported
// separately.
TEST_F(LorgnetteScannerManagerTest, DeduplicateNetPlusUsbScanner) {
  lorgnette::ListScannersResponse response;
  lorgnette::ScannerInfo info =
      CreateLorgnetteScanner(kLorgnetteUsbDeviceName, "MX3100");
  *response.add_scanners() = std::move(info);
  info = CreateLorgnetteScanner(kLorgnetteNetworkIpDeviceName, "MX3100");
  *response.add_scanners() = std::move(info);

  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(),
              ElementsAreArray(
                  {scanner.display_name, scanner.display_name + " (USB)"}));
}

// Test that a lorgnette scanner with a URL in the name gets reported as a
// network scanner instead of a USB scanner (i.e. USB is not in the returned
// scanner name).
TEST_F(LorgnetteScannerManagerTest, LorgnetteScannerWithUrl) {
  lorgnette::ListScannersResponse response =
      CreateListScannersResponse(kLorgnetteNetworkUrlDeviceName);
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  GetScannerNames();
  WaitForResult();
  auto& scanner = response.scanners()[0];
  std::string scanner_name = scanner.manufacturer() + " " + scanner.model();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner_name}));
}

// Test that detecting a lorgnette USB scanner results in a scanner name ending
// with "(USB)."
TEST_F(LorgnetteScannerManagerTest, LorgnetteUSBScanner) {
  lorgnette::ListScannersResponse response =
      CreateListScannersResponse(kLorgnetteUsbDeviceName);
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  GetScannerNames();
  WaitForResult();
  auto& scanner = response.scanners()[0];
  std::string scanner_name =
      scanner.manufacturer() + " " + scanner.model() + " (USB)";
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner_name}));
}

// Test that a lorgnette scanner whose model includes the manufacturer doesn't
// duplicate the manufacturer in the display name.
TEST_F(LorgnetteScannerManagerTest, LorgnetteScannerNoDuplicatedManufacturer) {
  lorgnette::ListScannersResponse response = CreateListScannersResponse(
      kLorgnetteNetworkIpDeviceName, kModelContainingManufacturer);
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  GetScannerNames();
  WaitForResult();
  const auto& scanner = response.scanners()[0];
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner.model()}));
}

// Test that two lorgnette scanners with the same manufacturer and model are
// given unique names.
TEST_F(LorgnetteScannerManagerTest, UniqueScannerNames) {
  lorgnette::ListScannersResponse response =
      CreateListScannersResponse(kLorgnetteNetworkIpDeviceName);
  lorgnette::ScannerInfo scanner =
      CreateLorgnetteScanner(kLorgnetteNetworkIpDeviceName);
  *response.add_scanners() = std::move(scanner);
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  GetScannerNames();
  WaitForResult();
  ASSERT_EQ(scanner_names().size(), 2u);
  EXPECT_NE(scanner_names()[0], scanner_names()[1]);
}

// Test that removing a detected scanner removes it from the list of available
// scanners.
TEST_F(LorgnetteScannerManagerTest, RemoveScanner) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_THAT(scanner_names(), ElementsAreArray({scanner.display_name}));
  fake_zeroconf_scanner_detector()->RemoveDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_TRUE(scanner_names().empty());
}

// Test that getting capabilities fails when GetScannerNames() has never been
// called.
TEST_F(LorgnetteScannerManagerTest, GetCapsNoScanner) {
  GetScannerCapabilities(kUnknownScannerName);
  WaitForResult();
  EXPECT_FALSE(scanner_capabilities());
}

// Test that getting capabilities fails when the scanner name does not
// correspond to a known scanner.
TEST_F(LorgnetteScannerManagerTest, GetCapsUnknownScanner) {
  fake_zeroconf_scanner_detector()->AddDetections({CreateZeroconfScanner()});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  GetScannerCapabilities(kUnknownScannerName);
  WaitForResult();
  EXPECT_FALSE(scanner_capabilities());
}

// Test that getting capabilities fails when there is no usable device name.
TEST_F(LorgnetteScannerManagerTest, GetCapsNoUsableDeviceName) {
  auto scanner = CreateZeroconfScanner(/*usable=*/false);
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  GetScannerCapabilities(scanner.display_name);
  WaitForResult();
  EXPECT_FALSE(scanner_capabilities());
}

// Test that failing to get capabilities from lorgnette returns no capabilities.
TEST_F(LorgnetteScannerManagerTest, GetCapsFail) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  GetLorgnetteManagerClient()->SetScannerCapabilitiesResponse(absl::nullopt);
  GetScannerCapabilities(scanner.display_name);
  WaitForResult();
  EXPECT_FALSE(scanner_capabilities());
}

// Test that getting capabilities succeeds with a valid scanner name.
TEST_F(LorgnetteScannerManagerTest, GetCaps) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  lorgnette::ScannerCapabilities capabilities;
  capabilities.add_resolutions(300);
  capabilities.add_color_modes(lorgnette::MODE_COLOR);
  GetLorgnetteManagerClient()->SetScannerCapabilitiesResponse(capabilities);
  GetScannerNames();
  WaitForResult();
  GetScannerCapabilities(scanner.display_name);
  WaitForResult();
  ASSERT_TRUE(scanner_capabilities());
  const auto caps = scanner_capabilities().value();
  ASSERT_EQ(caps.resolutions_size(), 1);
  EXPECT_EQ(caps.resolutions()[0], 300u);
  EXPECT_EQ(caps.sources_size(), 0);
  ASSERT_EQ(caps.color_modes_size(), 1);
  EXPECT_EQ(caps.color_modes()[0], lorgnette::MODE_COLOR);
}

// Test that scanning fails when GetScannerNames() has never been called.
TEST_F(LorgnetteScannerManagerTest, NoScannersNames) {
  lorgnette::ScanSettings settings;
  Scan(kUnknownScannerName, settings);
  WaitForResult();
  EXPECT_EQ(scan_data().size(), 0u);
  EXPECT_EQ(failure_mode(), lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
}

// Test that scanning fails when the scanner name does not correspond to a known
// scanner.
TEST_F(LorgnetteScannerManagerTest, UnknownScannerName) {
  fake_zeroconf_scanner_detector()->AddDetections({CreateZeroconfScanner()});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  lorgnette::ScanSettings settings;
  Scan(kUnknownScannerName, settings);
  WaitForResult();
  EXPECT_EQ(scan_data().size(), 0u);
  EXPECT_EQ(failure_mode(), lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
}

// Test that scanning fails when there is no usable device name.
TEST_F(LorgnetteScannerManagerTest, NoUsableDeviceName) {
  auto scanner = CreateZeroconfScanner(/*usable=*/false);
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  lorgnette::ScanSettings settings;
  Scan(scanner.display_name, settings);
  WaitForResult();
  EXPECT_EQ(scan_data().size(), 0u);
  EXPECT_EQ(failure_mode(), lorgnette::SCAN_FAILURE_MODE_UNKNOWN);
}

// Test that images aren't rotated when scanner isn't an Epson scanner.
TEST_F(LorgnetteScannerManagerTest, ScanNotRotatedNonEpson) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_FALSE(GetRotateAlternate(scanner.display_name, "ADF Duplex"));
}

// Test that images aren't rotated when scanner is a non-rotating Epson scanner.
TEST_F(LorgnetteScannerManagerTest, ScanNotRotatedEpsonException) {
  auto scanner = CreateScannerCustomName("Epson WF-C579Ra");
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_FALSE(GetRotateAlternate(scanner.display_name, "ADF Duplex"));
}

// Test that images aren't rotated when scan request is non-ADF.
TEST_F(LorgnetteScannerManagerTest, ScanNotRotatedNonADF) {
  auto scanner = CreateScannerCustomName("Epson XP-7100");
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_FALSE(GetRotateAlternate(scanner.display_name, "Flatbed"));
}

// Test that scanned images are rotated when scanner setup requires it.
TEST_F(LorgnetteScannerManagerTest, ScanRotated) {
  auto scanner = CreateScannerCustomName("Epson XP-7100");
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  EXPECT_TRUE(GetRotateAlternate(scanner.display_name, "ADF Duplex"));
}

// Test that scanning succeeds with a valid scanner name.
TEST_F(LorgnetteScannerManagerTest, ScanOnePage) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  std::vector<std::string> pages = {"TestScanData"};
  GetLorgnetteManagerClient()->SetScanResponse(pages);
  lorgnette::ScanSettings settings;
  Scan(scanner.display_name, settings);
  WaitForResult();
  ASSERT_EQ(scan_data().size(), 1u);
  EXPECT_EQ(scan_data()[0], "TestScanData");
  EXPECT_EQ(failure_mode(), lorgnette::SCAN_FAILURE_MODE_NO_FAILURE);
}

TEST_F(LorgnetteScannerManagerTest, ScanMultiplePages) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  std::vector<std::string> pages = {"TestPageOne", "TestPageTwo",
                                    "TestPageThree"};
  GetLorgnetteManagerClient()->SetScanResponse(pages);
  lorgnette::ScanSettings settings;
  Scan(scanner.display_name, settings);
  WaitForResult();
  ASSERT_EQ(scan_data().size(), 3u);
  EXPECT_EQ(scan_data()[0], "TestPageOne");
  EXPECT_EQ(scan_data()[1], "TestPageTwo");
  EXPECT_EQ(scan_data()[2], "TestPageThree");
  EXPECT_EQ(failure_mode(), lorgnette::SCAN_FAILURE_MODE_NO_FAILURE);
}

// Test that requesting to cancel the current scan job returns the success
// result.
TEST_F(LorgnetteScannerManagerTest, CancelScan) {
  auto scanner = CreateZeroconfScanner();
  fake_zeroconf_scanner_detector()->AddDetections({scanner});
  CompleteTasks();
  GetScannerNames();
  WaitForResult();
  CancelScan();
  WaitForResult();
  EXPECT_TRUE(cancel_scan_success());
}

}  // namespace ash
