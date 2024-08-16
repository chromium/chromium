// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/printing/zeroconf_printer_detector.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/hash/md5.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

// Supported service names for printers.
const char ZeroconfPrinterDetector::kIppServiceName[] = "_ipp._tcp.local";
const char ZeroconfPrinterDetector::kIppsServiceName[] = "_ipps._tcp.local";
const char ZeroconfPrinterDetector::kSocketServiceName[] =
    "_pdl-datastream._tcp.local";
const char ZeroconfPrinterDetector::kLpdServiceName[] = "_printer._tcp.local";

// IppEverywhere printers are also required to advertise these services.
const char ZeroconfPrinterDetector::kIppEverywhereServiceName[] =
    "_print._sub._ipp._tcp.local";
const char ZeroconfPrinterDetector::kIppsEverywhereServiceName[] =
    "_print._sub._ipps._tcp.local";

// These service names are ordered in priority. In other words, earlier
// service types in this list will be used preferentially over later ones.
constexpr std::array<const char*, 6> kServiceNames = {
    ZeroconfPrinterDetector::kIppsEverywhereServiceName,
    ZeroconfPrinterDetector::kIppEverywhereServiceName,
    ZeroconfPrinterDetector::kIppsServiceName,
    ZeroconfPrinterDetector::kIppServiceName,
    ZeroconfPrinterDetector::kSocketServiceName,
    ZeroconfPrinterDetector::kLpdServiceName,
};

// Certain printers advertise IPP/IPPS but are known not to work with that
// protocol.  Don't allow IPP/IPPS connections for printers in this list.
// Printers in this list should be all lowercase.  See b/268531843 for more
// context.
constexpr auto kIppRejectList = base::MakeFixedFlatSet<std::string_view>({
    "brother mfc-9340cdw",
    "canon e480 series",
    "canon ib4000 series",
    "canon mb2000 series",
    "canon mb2300 series",
    "canon mb5000 series",
    "canon mb5300 series",
    "canon mg3000 series",
    "canon mx490 series",
});

namespace {

using local_discovery::ServiceDescription;
using local_discovery::ServiceDiscoveryDeviceLister;
using local_discovery::ServiceDiscoverySharedClient;

// These (including the default values) come from section 9.2 of the Bonjour
// Printing Spec v1.2, and the field names follow the spec definitions instead
// of the canonical Chromium style.
//
// Not all of these will necessarily be specified for a given printer.  Also, we
// only define the fields that we care about, others not listed here we just
// ignore.
class ParsedMetadata {
 public:
  std::string adminurl;
  std::string air = "none";
  std::string note;
  std::string pdl = "application/postscript";
  // We stray slightly from the spec for product.  In the bonjour spec, product
  // is always enclosed in parentheses because...reasons.  We strip out parens.
  std::string product;
  std::string rp;
  std::string ty;
  std::string usb_MDL;
  std::string usb_MFG;
  std::string UUID;

  // Parse out metadata from sd to fill this structure.
  explicit ParsedMetadata(const ServiceDescription& sd) {
    for (const std::string& m : sd.metadata) {
      auto parts = base::SplitStringOnce(m, '=');
      if (!parts) {
        continue;
      }
      auto [key, value] = *parts;
      if (key == "note") {
        note = std::string(value);
      } else if (key == "pdl") {
        pdl = std::string(value);
      } else if (key == "product") {
        // Strip parens; ignore anything not enclosed in parens as malformed.
        if (base::StartsWith(value, "(") && base::EndsWith(value, ")")) {
          product = std::string(value.substr(1, value.size() - 2));
        }
      } else if (key == "rp") {
        rp = std::string(value);
      } else if (key == "ty") {
        ty = std::string(value);
      } else if (key == "usb_MDL") {
        usb_MDL = std::string(value);
      } else if (key == "usb_MFG") {
        usb_MFG = std::string(value);
      } else if (key == "UUID") {
        UUID = std::string(value);
      }
    }
  }
  ParsedMetadata(const ParsedMetadata& other) = delete;
};

// Create a unique identifier for this printer based on the ServiceDescription.
// This is what is used to determine whether or not this is the same printer
// when seen again later.  We use an MD5 hash of fields we expect to be
// immutable.
//
// These ids are persistent in synced storage; if you change this function
// carelessly, you will create mismatches between users' stored printer
// configurations and the printers themselves.
//
// Note we explicitly *don't* use the service type in this hash, because the
// same printer may export multiple services (ipp and ipps), and we want them
// all to be considered the same printer.
std::string ZeroconfPrinterId(const ServiceDescription& service,
                              const ParsedMetadata& metadata) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, service.instance_name());
  base::MD5Update(&ctx, metadata.product);
  base::MD5Update(&ctx, metadata.UUID);
  base::MD5Update(&ctx, metadata.usb_MFG);
  base::MD5Update(&ctx, metadata.usb_MDL);
  base::MD5Update(&ctx, metadata.ty);
  base::MD5Update(&ctx, metadata.rp);
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return base::StringPrintf("zeroconf-%s",
                            base::MD5DigestToBase16(digest).c_str());
}

// Attempt to fill |detected_printer| using the information in
// |service_description| and |metadata|.  Return true on success, false on
// failure.
bool ConvertToPrinter(const std::string& service_type,
                      const ServiceDescription& service_description,
                      const ParsedMetadata& metadata,
                      PrinterDetector::DetectedPrinter* detected_printer) {
  // If we don't have the minimum information needed to attempt a setup, fail.
  // Also fail on a port of 0, as this is used to indicate that the service
  // doesn't *actually* exist, the device just wants to guard the name.
  if (service_description.service_name.empty()) {
    PRINTER_LOG(ERROR) << "Found zeroconf " << service_type
                       << " printer with missing service name.";
    return false;
  }
  if (service_description.ip_address.empty()) {
    PRINTER_LOG(ERROR) << "Found zeroconf " << service_type
                       << " printer named '" << service_description.service_name
                       << "' with missing IP address.";
    return false;
  }
  if (service_description.address.port() == 0) {
    PRINTER_LOG(ERROR) << "Found zeroconf " << service_type
                       << " printer named '" << service_description.service_name
                       << "' with invalid port.";
    return false;
  }

  chromeos::Printer& printer = detected_printer->printer;
  printer.set_id(ZeroconfPrinterId(service_description, metadata));
  printer.set_uuid(metadata.UUID);
  printer.set_display_name(service_description.instance_name());
  printer.set_description(metadata.note);
  printer.set_make_and_model(metadata.ty);
  chromeos::Uri uri;
  std::string rp = metadata.rp;
  if (service_type == ZeroconfPrinterDetector::kIppServiceName ||
      service_type == ZeroconfPrinterDetector::kIppEverywhereServiceName) {
    uri.SetScheme("ipp");
  } else if (service_type == ZeroconfPrinterDetector::kIppsServiceName ||
             service_type ==
                 ZeroconfPrinterDetector::kIppsEverywhereServiceName) {
    uri.SetScheme("ipps");
  } else if (service_type == ZeroconfPrinterDetector::kSocketServiceName) {
    uri.SetScheme("socket");
    // Bonjour Printing Specification v1.2.1 section 9.2.2:
    // If the "rp" key is present in a Socket TXT record, the key/value MUST
    // be ignored.
    rp.clear();
  } else if (service_type == ZeroconfPrinterDetector::kLpdServiceName) {
    uri.SetScheme("lpd");
  } else {
    // Since we only register for these services, we should never get back
    // a service other than the ones above.
    NOTREACHED_IN_MIGRATION() << "Zeroconf printer with unknown service type "
                              << service_description.service_type();
    return false;
  }

  if (!uri.SetHostEncoded(service_description.address.HostForURL()) ||
      !uri.SetPort(service_description.address.port()) ||
      !uri.SetPathEncoded("/" + rp) || !printer.SetUri(uri)) {
    PRINTER_LOG(ERROR) << "Zeroconf printer type " << service_type << " named '"
                       << service_description.instance_name()
                       << "' has invalid uri: " << uri.GetNormalized();
    return false;
  }

  // Per the IPP Everywhere Standard 5100.14-2013, section 4.2.1, IPP
  // everywhere-capable printers advertise services prefixed with "_print"
  // (possibly in addition to prefix-free versions).  If we get a printer from a
  // _print service type, it should be auto-configurable with IPP Everywhere.
  printer.mutable_ppd_reference()->autoconf =
      base::StartsWith(service_type, "_print._sub");

  // Gather ppd identification candidates.
  detected_printer->ppd_search_data.discovery_type =
      chromeos::PrinterSearchData::PrinterDiscoveryType::kZeroconf;
  if (!metadata.ty.empty()) {
    detected_printer->ppd_search_data.make_and_model.push_back(metadata.ty);
  }
  if (!metadata.product.empty()) {
    detected_printer->ppd_search_data.make_and_model.push_back(
        metadata.product);
  }
  if (!metadata.usb_MFG.empty() && !metadata.usb_MDL.empty()) {
    detected_printer->ppd_search_data.make_and_model.push_back(
        base::StringPrintf("%s %s", metadata.usb_MFG.c_str(),
                           metadata.usb_MDL.c_str()));
  }
  if (!metadata.pdl.empty()) {
    // Per Bonjour Printer Spec v1.2 section 9.2.8, it is invalid for the pdl to
    // end with a comma.
    auto media_types = base::SplitStringPiece(
        metadata.pdl, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (!media_types.empty() && !media_types.back().empty()) {
      // Prune any empty splits.
      std::erase_if(media_types, [](std::string_view s) { return s.empty(); });

      base::ranges::transform(
          media_types,
          std::back_inserter(
              detected_printer->ppd_search_data.supported_document_formats),
          [](std::string_view s) { return base::ToLowerASCII(s); });
    }
  }

  PRINTER_LOG(EVENT) << "Found zeroconf " << service_type << " printer named '"
                     << service_description.instance_name() << "' at "
                     << uri.GetNormalized();
  return true;
}

class ZeroconfPrinterDetectorImpl : public ZeroconfPrinterDetector {
 public:
  // Normal constructor, connects to service discovery.
  ZeroconfPrinterDetectorImpl()
      : discovery_client_(ServiceDiscoverySharedClient::GetInstance()),
        reject_ipp_printers_(kIppRejectList.begin(), kIppRejectList.end()) {
    for (const char* service_type : kServiceNames) {
      CreateDeviceLister(service_type);
    }
  }

  // Testing constructor, uses injected backends.
  explicit ZeroconfPrinterDetectorImpl(
      std::map<std::string, std::unique_ptr<ServiceDiscoveryDeviceLister>>*
          device_listers,
      base::flat_set<std::string> ipp_reject_list)
      : reject_ipp_printers_(std::move(ipp_reject_list)) {
    device_listers_.swap(*device_listers);
    for (auto& entry : device_listers_) {
      entry.second->Start();
      entry.second->DiscoverNewDevices();
    }
  }

  ~ZeroconfPrinterDetectorImpl() override {}

  // PrinterDetector override.
  void RegisterPrintersFoundCallback(OnPrintersFoundCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    DCHECK(!on_printers_found_callback_);
    on_printers_found_callback_ = std::move(cb);
  }

  // PrinterDetector override.
  std::vector<DetectedPrinter> GetPrinters() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    base::AutoLock auto_lock(printers_lock_);
    return GetPrintersLocked();
  }

  // ServiceDiscoveryDeviceLister::Delegate implementation
  void OnDeviceChanged(const std::string& service_type,
                       bool added,
                       const ServiceDescription& service_description) override {
    // We don't care if it was added or not; we generate an update either way.
    ParsedMetadata metadata(service_description);
    DetectedPrinter printer;
    if (!ConvertToPrinter(service_type, service_description, metadata,
                          &printer)) {
      return;
    }
    if ((service_type == kIppServiceName || service_type == kIppsServiceName)) {
      const std::string lowercase_key =
          base::ToLowerASCII(printer.printer.make_and_model());
      if (reject_ipp_printers_.contains(lowercase_key)) {
        PRINTER_LOG(EVENT) << "Rejecting " << lowercase_key
                           << " for service type " << service_type;
        return;
      }
    }
    base::AutoLock auto_lock(printers_lock_);
    printers_[service_type][service_description.instance_name()] = printer;
    if (on_printers_found_callback_) {
      on_printers_found_callback_.Run(GetPrintersLocked());
    }
  }

  // ServiceDiscoveryDeviceLister::Delegate implementation.  Remove the
  // given device if we know about it.
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override {
    // Leverage ServiceDescription parsing to pull out the instance name.
    ServiceDescription service_description;
    service_description.service_name = service_name;
    base::AutoLock auto_lock(printers_lock_);
    auto& service_type_map = printers_[service_type];
    auto it = service_type_map.find(service_description.instance_name());
    if (it != service_type_map.end()) {
      PRINTER_LOG(EVENT) << "Removed zeroconf printer type " << service_type
                         << " named " << service_name;
      service_type_map.erase(it);
      if (on_printers_found_callback_) {
        on_printers_found_callback_.Run(GetPrintersLocked());
      }
    } else {
      LOG(WARNING) << "Device removal requested for unknown '" << service_name
                   << "'";
    }
  }

  // Remove all devices that originated on all services types, and request
  // a new round of discovery. We clear all printers to prevent
  // |on_printers_found_callback| from returning stale cached printers.
  void OnDeviceCacheFlushed(const std::string& service_type) override {
    base::AutoLock auto_lock(printers_lock_);
    if (!IsPrintersEmpty()) {
      ClearPrinters();
      if (on_printers_found_callback_) {
        on_printers_found_callback_.Run(GetPrintersLocked());
      }
    }

    // Request a new round of discovery from the lister.
    auto lister_entry = device_listers_.find(service_type);
    DCHECK(lister_entry != device_listers_.end());
    lister_entry->second->DiscoverNewDevices();
  }

  void OnPermissionRejected() override {}

  // Create a new device lister for the given |service_type| and add it
  // to the ones managed by this object.
  void CreateDeviceLister(const std::string& service_type) {
    auto lister = ServiceDiscoveryDeviceLister::Create(
        this, discovery_client_.get(), service_type);
    lister->Start();
    lister->DiscoverNewDevices();
    DCHECK(!base::Contains(device_listers_, service_type));
    device_listers_[service_type] = std::move(lister);
  }

 private:
  // Requires that printers_lock_ be held.
  std::vector<DetectedPrinter> GetPrintersLocked() {
    printers_lock_.AssertAcquired();
    std::map<std::string, DetectedPrinter> unified;
    // The order in which we look through these maps defines priority -- earlier
    // service types in this list will be used preferentially over later ones.
    // This depends on the fact that map::insert will fail if the entry already
    // exists.
    for (const char* service_type : kServiceNames) {
      for (const auto& entry : printers_[service_type]) {
        unified.insert({entry.first, entry.second});
      }
    }
    std::vector<DetectedPrinter> ret;
    ret.reserve(printers_.size());
    for (const auto& entry : unified) {
      ret.push_back(entry.second);
    }
    return ret;
  }

  // Clear all printers for every service type.
  void ClearPrinters() {
    printers_lock_.AssertAcquired();
    for (const char* service_type : kServiceNames) {
      printers_[service_type].clear();
    }
  }

  // Returns true if all the service names in |printers_| are empty.
  bool IsPrintersEmpty() const {
    printers_lock_.AssertAcquired();
    for (const char* service_type : kServiceNames) {
      DCHECK(base::Contains(printers_, service_type));
      if (!printers_.at(service_type).empty()) {
        return false;
      }
    }
    return true;
  }

  SEQUENCE_CHECKER(sequence_);

  // Map from service type to map from instance name to associated known
  // printer, and associated lock.
  std::map<std::string, std::map<std::string, DetectedPrinter>> printers_;
  base::Lock printers_lock_;

  // Keep a reference to the shared device client around for the lifetime of
  // this object.
  scoped_refptr<ServiceDiscoverySharedClient> discovery_client_;
  // Map from service_type to associated lister.
  std::map<std::string, std::unique_ptr<ServiceDiscoveryDeviceLister>>
      device_listers_;

  OnPrintersFoundCallback on_printers_found_callback_;

  // A set of printers known not to work with IPP/IPPS protocol.
  const base::flat_set<std::string> reject_ipp_printers_;
};

}  // namespace

// static
std::unique_ptr<ZeroconfPrinterDetector> ZeroconfPrinterDetector::Create() {
  return std::make_unique<ZeroconfPrinterDetectorImpl>();
}

// static
std::unique_ptr<ZeroconfPrinterDetector>
ZeroconfPrinterDetector::CreateForTesting(
    std::map<std::string, std::unique_ptr<ServiceDiscoveryDeviceLister>>*
        device_listers,
    base::flat_set<std::string> ipp_reject_list) {
  return std::make_unique<ZeroconfPrinterDetectorImpl>(
      device_listers, std::move(ipp_reject_list));
}

}  // namespace ash
