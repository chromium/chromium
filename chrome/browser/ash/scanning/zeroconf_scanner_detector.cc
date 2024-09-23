// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector_utils.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chromeos/ash/components/scanning/scanner.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

// Supported service types for scanners.
const char ZeroconfScannerDetector::kEsclServiceType[] = "_uscan._tcp.local";
const char ZeroconfScannerDetector::kEsclsServiceType[] = "_uscans._tcp.local";
const char ZeroconfScannerDetector::kGenericScannerServiceType[] =
    "_scanner._tcp.local";

constexpr std::array<const char*, 3> kServiceTypes = {
    ZeroconfScannerDetector::kEsclsServiceType,
    ZeroconfScannerDetector::kEsclServiceType,
    ZeroconfScannerDetector::kGenericScannerServiceType,
};

namespace {

using local_discovery::ServiceDescription;
using local_discovery::ServiceDiscoveryDeviceLister;
using local_discovery::ServiceDiscoverySharedClient;

// TODO(b/184746628): Update this class using the eSCL specification.
// These fields (including the default values) come from the eSCL specification.
// Not all of these will necessarily be specified for a given scanner. Also,
// unused fields are excluded here.
class ParsedMetadata {
 public:
  explicit ParsedMetadata(const ServiceDescription& service_description) {
    // Preference is to use mfg/mdl for the manufacturer and model.  If those
    // are not present in the metadata, attempt to set them using ty.
    std::string ty;
    for (const std::string& entry : service_description.metadata) {
      const std::string_view key_value(entry);
      const size_t equal_pos = key_value.find("=");
      if (equal_pos == std::string_view::npos) {
        continue;
      }

      const std::string_view key = key_value.substr(0, equal_pos);
      const std::string_view value = key_value.substr(equal_pos + 1);
      if (key == "rs") {
        rs_ = std::string(value);
      } else if (key == "usb_MFG" || key == "mfg") {
        manufacturer_ = value;
      } else if (key == "usb_MDL" || key == "mdl") {
        model_ = value;
      } else if (key == "ty") {
        ty = value;
      } else if (key == "UUID" || key == "uuid") {
        uuid_ = value;
      } else if (key == "pdl") {
        pdl_ = base::SplitString(value, ",", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
      }
    }

    // Both are already populated - nothing more needs to be done.
    if (!manufacturer_.empty() && !model_.empty()) {
      return;
    }

    if (ty.empty()) {
      return;
    }

    // If either |manufacturer_| or |model_| are not provided, use |ty| to
    // populate both.  In this case, assume the first word in |ty| is the
    // manufacturer and the rest is the model.
    auto space = ty.find(" ");
    manufacturer_ = ty.substr(0, space);
    model_ = (space == std::string::npos) ? "" : ty.substr(space + 1);
    // Trim whitespace here in case there are multiple spaces between
    // manufacturer and model.
    base::TrimWhitespaceASCII(model_, base::TRIM_ALL, &model_);
  }
  ParsedMetadata(const ParsedMetadata&) = delete;
  ParsedMetadata& operator=(const ParsedMetadata&) = delete;
  ~ParsedMetadata() = default;

  const std::optional<std::string>& rs() const { return rs_; }
  const std::string& manufacturer() const { return manufacturer_; }
  const std::string& model() const { return model_; }
  const std::string& uuid() const { return uuid_; }
  const std::vector<std::string>& pdl() const { return pdl_; }

 private:
  // Used to construct the path for a device name URL.
  std::optional<std::string> rs_;
  std::string manufacturer_;
  std::string model_;
  std::string uuid_;
  std::vector<std::string> pdl_;
};

// Some scanners return zeroconf responses for multiple protocols where some
// protocols are known to work better than others.  This function looks at
// |service_type| and |metadata| and returns true if this record represents a
// protocol/device combination that should be skipped.
bool ShouldSkipZeroconfScanner(const std::string& service_type,
                               const ParsedMetadata& metadata) {
  if (service_type != ZeroconfScannerDetector::kGenericScannerServiceType) {
    return false;
  }

  if (metadata.manufacturer() == "EPSON") {
    // Prefer eSCL for XP-7100 (b/288301496).
    if (metadata.model().find("XP-7100") != std::string::npos) {
      return true;
    }
  }

  return false;
}

// Attempts to create a Scanner using the information in |service_description|
// and |metadata|. Returns the Scanner on success, std::nullopt on failure.
std::optional<Scanner> CreateScanner(
    const std::string& service_type,
    const ServiceDescription& service_description,
    const ParsedMetadata& metadata) {
  // If there isn't enough information available to interact with the scanner,
  // fail. Also fail if the port number is 0, as this is used to indicate that
  // the service doesn't *actually* exist, the device just wants to guard the
  // name.
  if (service_description.service_name.empty() ||
      service_description.ip_address.empty() ||
      service_description.address.port() == 0) {
    PRINTER_LOG(ERROR) << "Found zeroconf " << service_type
                       << " scanner that isn't usable: "
                       << service_description.service_name << "("
                       << service_description.address.ToString() << ")";
    return std::nullopt;
  }

  if (ShouldSkipZeroconfScanner(service_type, metadata)) {
    PRINTER_LOG(DEBUG) << "Skipped zeroconf " << service_type
                       << " scanner named '"
                       << service_description.instance_name() << "' at "
                       << service_description.address.ToString();
    return std::nullopt;
  }

  PRINTER_LOG(EVENT) << "Found zeroconf " << service_type << " scanner named '"
                     << service_description.instance_name() << "' at "
                     << service_description.address.ToString();

  return CreateSaneScanner(service_description.instance_name(), service_type,
                           metadata.manufacturer(), metadata.model(),
                           metadata.uuid(), metadata.rs(), metadata.pdl(),
                           service_description.ip_address,
                           service_description.address.port());
}

class ZeroconfScannerDetectorImpl final : public ZeroconfScannerDetector {
 public:
  // Normal constructor that connects to service discovery.
  ZeroconfScannerDetectorImpl()
      : discovery_client_(ServiceDiscoverySharedClient::GetInstance()) {}

  // Testing constructor that uses injected backends.
  explicit ZeroconfScannerDetectorImpl(ListersMap&& device_listers) {
    device_listers_ = std::move(device_listers);
    for (auto& lister : device_listers_) {
      lister.second->Start();
      lister.second->DiscoverNewDevices();
    }
  }

  ZeroconfScannerDetectorImpl(const ZeroconfScannerDetectorImpl&) = delete;
  ZeroconfScannerDetectorImpl& operator=(const ZeroconfScannerDetectorImpl&) =
      delete;
  ~ZeroconfScannerDetectorImpl() override = default;

  // Initializes the detector by creating its device listers.
  void Init() {
    for (const char* service_type : kServiceTypes)
      CreateDeviceLister(service_type);
  }

  // ScannerDetector:
  void RegisterScannersDetectedCallback(
      OnScannersDetectedCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    DCHECK(!on_scanners_detected_callback_);
    on_scanners_detected_callback_ = std::move(callback);
  }

  // ScannerDetector:
  std::vector<Scanner> GetScanners() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    return GetDedupedScanners();
  }

  // ServiceDiscoveryDeviceLister::Delegate:
  void OnDeviceChanged(const std::string& service_type,
                       bool added,
                       const ServiceDescription& service_description) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    // Generate an update whether the device was added or not.
    ParsedMetadata metadata(service_description);
    auto scanner = CreateScanner(service_type, service_description, metadata);
    if (!scanner.has_value())
      return;

    scanners_[service_description.service_name] = scanner.value();
    if (on_scanners_detected_callback_)
      on_scanners_detected_callback_.Run(GetDedupedScanners());
  }

  // ServiceDiscoveryDeviceLister::Delegate:
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (scanners_.erase(service_name)) {
      if (on_scanners_detected_callback_)
        on_scanners_detected_callback_.Run(GetDedupedScanners());
    } else {
      LOG(WARNING) << "Device removal requested for unknown service: "
                   << service_name;
    }
  }

  // ServiceDiscoveryDeviceLister::Delegate:
  // Removes all devices that originated on all service types and requests a new
  // round of discovery. Clears all scanners to avoid returning stale cached
  // scanners.
  void OnDeviceCacheFlushed(const std::string& service_type) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!scanners_.empty()) {
      scanners_.clear();
      if (on_scanners_detected_callback_)
        on_scanners_detected_callback_.Run(GetDedupedScanners());
    }

    // Request a new round of discovery from the lister.
    auto lister_entry = device_listers_.find(service_type);
    DCHECK(lister_entry != device_listers_.end());
    lister_entry->second->DiscoverNewDevices();
  }

  void OnPermissionRejected() override {}

 private:
  // Creates a new device lister for the given |service_type| and adds it to the
  // ones managed by this object.
  void CreateDeviceLister(const std::string& service_type) {
    auto lister = ServiceDiscoveryDeviceLister::Create(
        this, discovery_client_.get(), service_type);
    lister->Start();
    lister->DiscoverNewDevices();
    DCHECK(!base::Contains(device_listers_, service_type));
    device_listers_[service_type] = std::move(lister);
  }

  // Returns the detected scanners after merging duplicates.
  std::vector<Scanner> GetDedupedScanners() {
    // Use a map of display name to Scanner to deduplicate the detected
    // scanners. If a Scanner has the same display name as one that's already
    // been added to the map, merge the two by adding the new Scanner's
    // information to the existing Scanner.
    base::flat_map<std::string, Scanner> deduped_scanners;
    for (const auto& entry : scanners_) {
      const Scanner* scanner = &entry.second;
      auto it = deduped_scanners.find(scanner->display_name);
      if (it == deduped_scanners.end()) {
        deduped_scanners.insert({scanner->display_name, *scanner});
      } else {
        // Each Scanner in scanners_ should have a single device name
        // corresponding to a known protocol.
        ScanProtocol protocol = ScanProtocol::kUnknown;
        if (scanner->device_names.find(ScanProtocol::kEscls) !=
            scanner->device_names.end()) {
          protocol = ScanProtocol::kEscls;
        } else if (scanner->device_names.find(ScanProtocol::kEscl) !=
                   scanner->device_names.end()) {
          protocol = ScanProtocol::kEscl;
        } else if (scanner->device_names.find(ScanProtocol::kLegacyNetwork) !=
                   scanner->device_names.end()) {
          protocol = ScanProtocol::kLegacyNetwork;
        } else {
          NOTREACHED_IN_MIGRATION()
              << "Zeroconf scanner with unknown protocol.";
        }

        it->second.device_names[protocol].insert(
            scanner->device_names.at(protocol).begin(),
            scanner->device_names.at(protocol).end());
        it->second.ip_addresses.insert(scanner->ip_addresses.begin(),
                                       scanner->ip_addresses.end());
      }
    }

    std::vector<Scanner> scanners;
    scanners.reserve(deduped_scanners.size());
    for (const auto& entry : deduped_scanners)
      scanners.push_back(entry.second);

    return scanners;
  }

  SEQUENCE_CHECKER(sequence_);

  // Map from service name to Scanner.
  base::flat_map<std::string, Scanner> scanners_;

  // Keep a reference to the shared device client around for the lifetime of
  // this object.
  scoped_refptr<ServiceDiscoverySharedClient> discovery_client_;

  // Map from service_type to associated lister.
  ListersMap device_listers_;

  // Callback used to notify when scanners are detected.
  OnScannersDetectedCallback on_scanners_detected_callback_;
};

}  // namespace

// static
std::unique_ptr<ZeroconfScannerDetector> ZeroconfScannerDetector::Create() {
  std::unique_ptr<ZeroconfScannerDetectorImpl> detector =
      std::make_unique<ZeroconfScannerDetectorImpl>();
  detector->Init();
  return std::move(detector);
}

// static
std::unique_ptr<ZeroconfScannerDetector>
ZeroconfScannerDetector::CreateForTesting(ListersMap&& device_listers) {
  return std::make_unique<ZeroconfScannerDetectorImpl>(
      std::move(device_listers));
}

}  // namespace ash
