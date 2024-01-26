// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector_utils.h"
#include "chrome/browser/local_discovery/fake_service_discovery_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "chromeos/ash/components/scanning/scanner.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using local_discovery::FakeServiceDiscoveryDeviceLister;
using local_discovery::ServiceDescription;
using local_discovery::ServiceDiscoveryDeviceLister;

MATCHER_P(ScannerIsEqual, expected_scanner, "") {
  return arg.display_name == expected_scanner.display_name &&
         arg.manufacturer == expected_scanner.manufacturer &&
         arg.model == expected_scanner.model &&
         arg.uuid == expected_scanner.uuid && arg.pdl == expected_scanner.pdl &&
         arg.device_names == expected_scanner.device_names &&
         arg.ip_addresses == expected_scanner.ip_addresses;
}

MATCHER_P(ScannersAreEqual, expected_scanners, "") {
  if (arg.size() != expected_scanners.size()) {
    return false;
  }

  std::vector<Scanner> sorted_expected = expected_scanners;
  std::vector<Scanner> sorted_actual = arg;
  std::sort(sorted_expected.begin(), sorted_expected.end(),
            [](const Scanner& a, const Scanner& b) -> bool {
              return a.display_name < b.display_name;
            });
  std::sort(sorted_actual.begin(), sorted_actual.end(),
            [](const Scanner& a, const Scanner& b) -> bool {
              return a.display_name < b.display_name;
            });
  for (size_t i = 0; i < sorted_expected.size(); ++i) {
    EXPECT_THAT(sorted_actual[i], ScannerIsEqual(sorted_expected[i]));
  }

  return true;
}

// TODO(b/184743530): Move these functions and the copies in
// zeroconf_printer_detector_unittest.cc into a shared file.
// Determine basic scanner attributes deterministically but pseudorandomly based
// on the scanner name. The exact values returned here are not important. The
// important parts are that there's variety based on the name, and it's
// deterministic.

// Gets an IP address for this scanner. The returned address may be IPv4 or
// IPv6.
net::IPAddress GetIPAddressFor(const std::string& name) {
  std::mt19937 rng(std::hash<std::string>()(name));
  if (rng() & 1) {
    // Give an IPv4 address.
    return net::IPAddress(rng(), rng(), rng(), rng());
  }

  // Give an IPv6 address.
  return net::IPAddress(rng(), rng(), rng(), rng(), rng(), rng(), rng(), rng(),
                        rng(), rng(), rng(), rng(), rng(), rng(), rng(), rng());
}

// Gets a port number for this scanner.
int GetPortFor(const std::string& name) {
  return (std::hash<std::string>()(name) % 1000) + 1;
}

// A class used to create an expected scanner, allowing the caller to specify
// only necessary values.  This corresponds to MakeServiceDescription()
// below. Given the same name and correct service type, this generates the
// expected Scanner that the ZeroconfScannerDetector should create when it gets
// the ServiceDescription created by MakeServiceDescription(). This needs to be
// kept in sync with MakeServiceDescription().
class ScannerBuilder {
 public:
  ScannerBuilder(const std::string& name, const std::string& service_type)
      : name_(name), service_type_(service_type) {}
  ~ScannerBuilder() = default;
  ScannerBuilder(const ScannerBuilder& other) = default;
  ScannerBuilder& operator=(const ScannerBuilder& other) = default;

  ScannerBuilder& WithManufacturerAndModel(const std::string& manufacturer,
                                           const std::string& model) {
    manufacturer_ = manufacturer;
    model_ = model;
    return *this;
  }
  ScannerBuilder& WithRs(const std::optional<std::string>& rs) {
    rs_ = rs;
    return *this;
  }
  ScannerBuilder& WithUuid(const std::string& uuid) {
    uuid_ = uuid;
    return *this;
  }
  ScannerBuilder& WithPdl(const std::vector<std::string>& pdl) {
    pdl_ = pdl;
    return *this;
  }
  Scanner Build() {
    const net::IPAddress ip_address = GetIPAddressFor(name_);
    const int port = GetPortFor(name_);
    auto scanner =
        CreateSaneScanner(name_, service_type_, manufacturer_, model_, uuid_,
                          rs_, pdl_, ip_address, port);
    CHECK(scanner.has_value());
    return scanner.value();
  }

 private:
  std::string name_;
  std::string service_type_;
  std::string manufacturer_;
  std::string model_;
  std::string uuid_;
  std::optional<std::string> rs_;
  std::vector<std::string> pdl_;
};

// Merges all of the Scanners in |scanners| into a single Scanner. Used to
// create the expected result of a scanner announced by more than one lister.
Scanner MergeScanners(const std::vector<Scanner>& scanners) {
  if (scanners.empty()) {
    return Scanner();
  }

  Scanner merged_scanner = scanners[0];
  for (auto it = std::next(scanners.begin()); it != scanners.end(); ++it) {
    merged_scanner.device_names.insert(it->device_names.begin(),
                                       it->device_names.end());
    merged_scanner.ip_addresses.insert(it->ip_addresses.begin(),
                                       it->ip_addresses.end());
  }

  return merged_scanner;
}

// Similar to MakeServiceDescription below except the caller can add arbitrary
// |metadata|.
ServiceDescription MakeServiceDescription(const std::string& name,
                                          const std::string& service_type,
                                          std::vector<std::string> metadata) {
  ServiceDescription service_description;
  service_description.service_name = base::StrCat({name, ".", service_type});
  service_description.address.set_host(base::StrCat({name, ".local"}));
  service_description.address.set_port(GetPortFor(name));
  service_description.ip_address = GetIPAddressFor(name);
  service_description.metadata = std::move(metadata);
  return service_description;
}

// Creates a deterministic ServiceDescription based on the service name and
// type. See the note on ScannerBuilder above. This must be kept in sync
// with ScannerBuilder.
ServiceDescription MakeServiceDescription(
    const std::string& name,
    const std::string& service_type,
    const std::optional<std::string>& rs) {
  std::vector<std::string> metadata;
  if (rs.has_value()) {
    metadata.push_back(base::StrCat({"rs=", rs.value()}));
  }
  return MakeServiceDescription(name, service_type, std::move(metadata));
}

}  // namespace

class ZeroconfScannerDetectorTest : public testing::Test {
 public:
  ZeroconfScannerDetectorTest() = default;
  ~ZeroconfScannerDetectorTest() override = default;

  void SetUp() override {
    auto* runner = task_environment_.GetMainThreadTaskRunner().get();
    auto escl_lister = std::make_unique<FakeServiceDiscoveryDeviceLister>(
        runner, ZeroconfScannerDetector::kEsclServiceType);
    escl_lister_ = escl_lister.get();
    auto escls_lister = std::make_unique<FakeServiceDiscoveryDeviceLister>(
        runner, ZeroconfScannerDetector::kEsclsServiceType);
    escls_lister_ = escls_lister.get();
    auto generic_scanner_lister =
        std::make_unique<FakeServiceDiscoveryDeviceLister>(
            runner, ZeroconfScannerDetector::kGenericScannerServiceType);
    generic_scanner_lister_ = generic_scanner_lister.get();
    listers_[ZeroconfScannerDetector::kEsclServiceType] =
        std::move(escl_lister);
    listers_[ZeroconfScannerDetector::kEsclsServiceType] =
        std::move(escls_lister);
    listers_[ZeroconfScannerDetector::kGenericScannerServiceType] =
        std::move(generic_scanner_lister);
  }

  void CreateDetector() {
    detector_ = ZeroconfScannerDetector::CreateForTesting(std::move(listers_));

    // Ownership of the previously allocated listers map is transferred to the
    // detector, so the unique_ptr values of the listers map are no longer valid
    // at this point. The lister's raw pointers are kept as separate members to
    // keep the lister fakes accessible after ownership is transferred into the
    // detector.
    listers_.clear();
    detector_->RegisterScannersDetectedCallback(
        base::BindRepeating(&ZeroconfScannerDetectorTest::OnScannersDetected,
                            base::Unretained(this)));
    escl_lister_->SetDelegate(detector_.get());
    escls_lister_->SetDelegate(detector_.get());
    generic_scanner_lister_->SetDelegate(detector_.get());
  }

  // ScannerDetector callback.
  void OnScannersDetected(std::vector<Scanner> scanners) {
    scanners_ = std::move(scanners);
  }

 protected:
  // Runs pending tasks regardless of delay.
  void CompleteTasks() { task_environment_.FastForwardUntilNoTasksRemain(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Device lister fakes. These are initialized when the test is constructed.
  // These pointers don't involve ownership; ownership of the listers starts
  // with this class in listers_ when the test starts and is transferred to
  // detector_ when the detector is created. Throughout, the listers remain
  // available to the test via these pointers.
  raw_ptr<FakeServiceDiscoveryDeviceLister, DanglingUntriaged> escl_lister_;
  raw_ptr<FakeServiceDiscoveryDeviceLister, DanglingUntriaged> escls_lister_;
  raw_ptr<FakeServiceDiscoveryDeviceLister, DanglingUntriaged>
      generic_scanner_lister_;

  // Detector under test.
  std::unique_ptr<ZeroconfScannerDetector> detector_;

  // Latest scanners received in OnScannersDetected().
  std::vector<Scanner> scanners_;

 private:
  // Temporary storage for the device listers, between the time the test is
  // constructed and the detector is created. Tests shouldn't access this
  // directly, use the *_lister_ variables instead.
  ZeroconfScannerDetector::ListersMap listers_;
};

// Test that an eSCL scanner can be detected.
TEST_F(ZeroconfScannerDetectorTest, EsclScanner) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, ""));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithRs("")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that an eSCLS scanner can be detected.
TEST_F(ZeroconfScannerDetectorTest, EsclsScanner) {
  escls_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclsServiceType, ""));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclsServiceType)
          .WithRs("")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that a generic Epson _scanner._tcp scanner can be detected.
TEST_F(ZeroconfScannerDetectorTest, EpsonGenericScanner) {
  generic_scanner_lister_->Announce(MakeServiceDescription(
      "EPSONScanner1", ZeroconfScannerDetector::kGenericScannerServiceType,
      ""));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("EPSONScanner1",
                     ZeroconfScannerDetector::kGenericScannerServiceType)
          .WithRs("")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that a generic non-Epson _scanner._tcp scanner is not listed.
TEST_F(ZeroconfScannerDetectorTest, NonEpsonGenericScanner) {
  generic_scanner_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kGenericScannerServiceType, ""));
  CreateDetector();
  CompleteTasks();
  EXPECT_TRUE(scanners_.empty());
}

// Test that the same scanner detected by two listers is merged into a single
// Scanner.
TEST_F(ZeroconfScannerDetectorTest, MergedScanner) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, ""));
  escls_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclsServiceType, ""));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {MergeScanners(
      {ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
           .WithRs("")
           .Build(),
       ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclsServiceType)
           .WithRs("")
           .Build()})};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that the same Epson scanner detected by three listers is merged into a
// single Scanner.
TEST_F(ZeroconfScannerDetectorTest, MergedEpsonScanner) {
  escl_lister_->Announce(MakeServiceDescription(
      "EPSONScanner1", ZeroconfScannerDetector::kEsclServiceType, ""));
  escls_lister_->Announce(MakeServiceDescription(
      "EPSONScanner1", ZeroconfScannerDetector::kEsclsServiceType, ""));
  generic_scanner_lister_->Announce(MakeServiceDescription(
      "EPSONScanner1", ZeroconfScannerDetector::kGenericScannerServiceType,
      ""));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {MergeScanners(
      {ScannerBuilder("EPSONScanner1",
                      ZeroconfScannerDetector::kEsclServiceType)
           .WithRs("")
           .Build(),
       ScannerBuilder("EPSONScanner1",
                      ZeroconfScannerDetector::kEsclsServiceType)
           .WithRs("")
           .Build(),
       ScannerBuilder("EPSONScanner1",
                      ZeroconfScannerDetector::kGenericScannerServiceType)
           .WithRs("")
           .Build()})};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that two separate scanners can be detected.
TEST_F(ZeroconfScannerDetectorTest, EsclAndEsclsScanners) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, ""));
  escls_lister_->Announce(MakeServiceDescription(
      "Scanner2", ZeroconfScannerDetector::kEsclsServiceType, ""));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithRs("")
          .Build(),
      ScannerBuilder("Scanner2", ZeroconfScannerDetector::kEsclsServiceType)
          .WithRs("")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that calling GetScanners() returns the same scanners reported in
// OnScannersDetected().
TEST_F(ZeroconfScannerDetectorTest, GetScanners) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, ""));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithRs("")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
  EXPECT_THAT(detector_->GetScanners(), ScannersAreEqual(scanners_));
}

// Test that the detector detects a scanner that is announced after its
// creation.
TEST_F(ZeroconfScannerDetectorTest, AnnounceAfterDetectorCreation) {
  CreateDetector();
  CompleteTasks();
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, ""));
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithRs("")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that failing to parse the service metadata is handled gracefully.
TEST_F(ZeroconfScannerDetectorTest, InvalidMetadata) {
  ServiceDescription service_description = MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, "");
  service_description.metadata = {"no_equal_sign"};
  escl_lister_->Announce(service_description);
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that a service without a service name does not get added as a detected
// scanner.
TEST_F(ZeroconfScannerDetectorTest, NoServiceName) {
  ServiceDescription service_description = MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, "");
  service_description.service_name = "";
  escl_lister_->Announce(service_description);
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that a service without an IP address does not get added as a detected
// scanner.
TEST_F(ZeroconfScannerDetectorTest, NoIpAddress) {
  ServiceDescription service_description = MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, "");
  service_description.ip_address = net::IPAddress();
  escl_lister_->Announce(service_description);
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that a service with a port number of 0 does not get added as a detected
// scanner.
TEST_F(ZeroconfScannerDetectorTest, PortIs0) {
  ServiceDescription service_description = MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, "");
  service_description.address.set_port(0);
  escl_lister_->Announce(service_description);
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that a valid "rs" value gets incorporated into the device name.
TEST_F(ZeroconfScannerDetectorTest, Rs) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, "test"));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithRs("test")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that providing no "rs" value results in the default path being used in
// the device name.
TEST_F(ZeroconfScannerDetectorTest, NoRs) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, std::nullopt));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that metadata with just ty works.
TEST_F(ZeroconfScannerDetectorTest, MetadataTy) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      std::vector<std::string>{"ty=Manufacturer   Model 123  "}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithManufacturerAndModel("Manufacturer", "Model 123")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that metadata with just mfg and mdl works.
TEST_F(ZeroconfScannerDetectorTest, MetadataMfgMdl) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      {"mfg=Real Manufacturer", "mdl=Model 123"}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithManufacturerAndModel("Real Manufacturer", "Model 123")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that metadata with ty and mfg works.  Since both mfg and mdl are not
// present, mfg will not be used and ty will be used.
TEST_F(ZeroconfScannerDetectorTest, MetadataTyMfg) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      {"ty=Maker Model 123", "mfg=Bad-Manufacturer"}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithManufacturerAndModel("Maker", "Model 123")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that metadata with ty and mdl works.  Since both mfg and mdl are not
// present, mdl will not be used and ty will be used.
TEST_F(ZeroconfScannerDetectorTest, MetadataTyMdl) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      {"ty=The Manufacturer Model 123", "mdl=Bad-Model"}));
  CreateDetector();
  CompleteTasks();
  // Note that if manufacturer in the ty string contains a space, only the first
  // word is stripped out for the manufacturer entry.  That's why mfg/mdl are
  // preferred when they are both present.
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithManufacturerAndModel("The", "Manufacturer Model 123")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Tests that metadata with ty, mfg, and mdl works.  mfg and mdl should be used
// instead of the ty info.
TEST_F(ZeroconfScannerDetectorTest, MetadataTyMfgMdl) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      {"ty=Bad-Manufacturer Bad-Model", "mfg=Manufacturer", "mdl=Model 123"}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithManufacturerAndModel("Manufacturer", "Model 123")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that metadata with just usb_MFG and usb_MDL works.
TEST_F(ZeroconfScannerDetectorTest, MetadataUsbMfgMdl) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      {"usb_MFG=Real Manufacturer", "usb_MDL=Model 123"}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithManufacturerAndModel("Real Manufacturer", "Model 123")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that metadata with ty and usb_MFG works.  Since both usb_MFG and usb_MDL
// are not present, usb_MFG will not be used and ty will be used.
TEST_F(ZeroconfScannerDetectorTest, MetadataUsbTyMfg) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      {"ty=Maker Model 123", "usb_MFG=Bad-Manufacturer"}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithManufacturerAndModel("Maker", "Model 123")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that metadata with ty and usb_MDL works.  Since both usb_MFG and usb_MDL
// are not present, usb_MDL will not be used and ty will be used.
TEST_F(ZeroconfScannerDetectorTest, MetadataUsbTyMdl) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      {"ty=The Manufacturer Model 123", "usb_MDL=Bad Model"}));
  CreateDetector();
  CompleteTasks();
  // Note that if manufacturer in the ty string contains a space, only the first
  // word is stripped out for the manufacturer entry.  That's why
  // usb_MFG/usb_MDL are preferred when they are both present.
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithManufacturerAndModel("The", "Manufacturer Model 123")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Tests that metadata with ty, usb_MFG, and usb_MDL works.  usb_MFG and usb_MDL
// should be used instead of the ty info.
TEST_F(ZeroconfScannerDetectorTest, MetadataUsbTyMfgMdl) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      {"ty=Bad-Manufacturer Bad-Model", "usb_MFG=The Manufacturer",
       "usb_MDL=Model 123"}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithManufacturerAndModel("The Manufacturer", "Model 123")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Tests that metadata with UUID works.
TEST_F(ZeroconfScannerDetectorTest, MetadataUuid) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      std::vector<std::string>{"UUID=12345-67890"}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithUuid("12345-67890")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Tests that metadata with uuid works.
TEST_F(ZeroconfScannerDetectorTest, MetadataUuidLowercase) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      std::vector<std::string>{"uuid=12345-67890"}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithUuid("12345-67890")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Tests that metadata with PDL works.
TEST_F(ZeroconfScannerDetectorTest, MetadataPdl) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType,
      std::vector<std::string>{"pdl=pdl-1,pdl-2"}));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithPdl({"pdl-1", "pdl-2"})
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that metadata for a skipped scanner doesn't get converted.
TEST_F(ZeroconfScannerDetectorTest, MetadataSkippedRecord) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kGenericScannerServiceType,
      {"mfg=EPSON", "mdl=XP-7100 Series"}));
  CreateDetector();
  CompleteTasks();
  EXPECT_TRUE(scanners_.empty());
}

// Test that a detected scanner can be removed.
TEST_F(ZeroconfScannerDetectorTest, RemoveAddedScanner) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, std::nullopt));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
  escl_lister_->Remove("Scanner1");
  CompleteTasks();
  expected_scanners.clear();
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that removing an undetected scanner is ignored.
TEST_F(ZeroconfScannerDetectorTest, RemoveUnaddedScanner) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, std::nullopt));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
  escl_lister_->Remove("Scanner2");
  CompleteTasks();
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that removing a scanner from only one of two listers it was announced on
// does not completely remove the scanner (i.e. it goes from being a merged
// scanner to a single unmerged scanner).
TEST_F(ZeroconfScannerDetectorTest, RemovePartOfMergedScanner) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, ""));
  escls_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclsServiceType, ""));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {MergeScanners(
      {ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
           .WithRs("")
           .Build(),
       ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclsServiceType)
           .WithRs("")
           .Build()})};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
  escl_lister_->Remove("Scanner1");
  CompleteTasks();
  expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclsServiceType)
          .WithRs("")
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
}

// Test that a cache flush correctly removes scanners.
TEST_F(ZeroconfScannerDetectorTest, CacheFlush) {
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, ""));
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner2", ZeroconfScannerDetector::kEsclServiceType, std::nullopt));
  escls_lister_->Announce(MakeServiceDescription(
      "Scanner3", ZeroconfScannerDetector::kEsclsServiceType, ""));
  escls_lister_->Announce(MakeServiceDescription(
      "Scanner4", ZeroconfScannerDetector::kEsclsServiceType, "test"));
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner5", ZeroconfScannerDetector::kEsclServiceType, std::nullopt));
  CreateDetector();
  CompleteTasks();
  std::vector<Scanner> expected_scanners = {
      ScannerBuilder("Scanner1", ZeroconfScannerDetector::kEsclServiceType)
          .WithRs("")
          .Build(),
      ScannerBuilder("Scanner2", ZeroconfScannerDetector::kEsclServiceType)
          .Build(),
      ScannerBuilder("Scanner3", ZeroconfScannerDetector::kEsclsServiceType)
          .WithRs("")
          .Build(),
      ScannerBuilder("Scanner4", ZeroconfScannerDetector::kEsclsServiceType)
          .WithRs("test")
          .Build(),
      ScannerBuilder("Scanner5", ZeroconfScannerDetector::kEsclServiceType)
          .Build()};
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));
  escls_lister_->Clear();
  CompleteTasks();
  // With the eSCLS lister cleared, all scanners should be cleared.
  expected_scanners.clear();
  EXPECT_THAT(scanners_, ScannersAreEqual(expected_scanners));

  // Discovery should have started after dealing with the cache flush.
  EXPECT_TRUE(escls_lister_->discovery_started());
}

// Verify tasks are cleaned up properly when the detector is destroyed.
TEST_F(ZeroconfScannerDetectorTest, DestroyedWithTasksPending) {
  CreateDetector();
  escl_lister_->Announce(MakeServiceDescription(
      "Scanner1", ZeroconfScannerDetector::kEsclServiceType, ""));
  // Run listers but don't run the delayed tasks.
  task_environment_.RunUntilIdle();
  detector_.reset();
  CompleteTasks();
  SUCCEED();
}

}  // namespace ash
