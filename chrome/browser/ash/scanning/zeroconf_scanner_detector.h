// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_ZEROCONF_SCANNER_DETECTOR_H_
#define CHROME_BROWSER_ASH_SCANNING_ZEROCONF_SCANNER_DETECTOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/ash/scanning/scanner_detector.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

namespace ash {

// Uses mDNS and DNS-SD to detect nearby networked scanners.
class ZeroconfScannerDetector
    : public ScannerDetector,
      public local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  // Service types used by the detector.
  static const char kEsclServiceType[];
  static const char kEsclsServiceType[];
  static const char kGenericScannerServiceType[];

  ~ZeroconfScannerDetector() override = default;

  static std::unique_ptr<ZeroconfScannerDetector> Create();

  // Creates an instance that uses the passed device listers instead of creating
  // its own. |device_listers| is a map from service type to lister and should
  // supply a lister for each of the service types used by the detector.
  // Ownership is taken of the listers map.
  using ListersMap = base::flat_map<
      std::string,
      std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister>>;
  static std::unique_ptr<ZeroconfScannerDetector> CreateForTesting(
      ListersMap&& device_listers);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_ZEROCONF_SCANNER_DETECTOR_H_
