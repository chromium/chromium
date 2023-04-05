// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_ZEROCONF_PRINTER_DETECTOR_H_
#define CHROME_BROWSER_ASH_PRINTING_ZEROCONF_PRINTER_DETECTOR_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "chrome/browser/ash/printing/printer_detector.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"

namespace ash {

// Use mDNS and DNS-SD to detect nearby networked printers.  This is sometimes
// called zeroconf, or Bonjour.  Or Rendezvous.
class ZeroconfPrinterDetector
    : public PrinterDetector,
      public local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  // Service types used by the detector.
  static const char kIppServiceName[];
  static const char kIppsServiceName[];
  static const char kIppEverywhereServiceName[];
  static const char kIppsEverywhereServiceName[];
  static const char kSocketServiceName[];
  static const char kLpdServiceName[];

  ~ZeroconfPrinterDetector() override = default;

  static std::unique_ptr<ZeroconfPrinterDetector> Create();

  // Create an instance that uses the passed device listers and reject list
  // instead of creating its own.  |device_listers| is a map from service type
  // to lister, and should supply a lister for each of the service names used by
  // the detector.  Ownership is taken of the map storage.  |ipp_reject_list| is
  // a set of printers that we will reject for IPP/IPPS.
  static std::unique_ptr<ZeroconfPrinterDetector> CreateForTesting(
      std::map<std::string,
               std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister>>*
          device_listers,
      base::flat_set<std::string> ipp_reject_list);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_ZEROCONF_PRINTER_DETECTOR_H_
