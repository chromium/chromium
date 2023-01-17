// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_IPP_CLIENT_INFO_CALCULATOR_H_
#define CHROME_BROWSER_ASH_PRINTING_IPP_CLIENT_INFO_CALCULATOR_H_

#include <memory>
#include <string>

#include "printing/mojom/print.mojom-forward.h"

namespace policy {
class DeviceAttributes;
}

namespace ash::printing {

// Calculates and keeps track of `client-info` values that could be sent to IPP
// printers to identify the print job origin.
class IppClientInfoCalculator {
 public:
  IppClientInfoCalculator() = default;
  virtual ~IppClientInfoCalculator() = default;

  // Returns an IPP `client-info` value containing OS version information.
  // Cannot be `nullptr`.
  virtual ::printing::mojom::IppClientInfoPtr GetOsInfo() const = 0;

  // Returns an IPP `client-info` value corresponding to the current value of
  // the `kDevicePrintingClientNameTemplate` policy. Returns `nullptr` if
  // the policy is not set.
  virtual ::printing::mojom::IppClientInfoPtr GetDeviceInfo() const = 0;

  static std::unique_ptr<IppClientInfoCalculator> Create();

  // Factory function that allows injected dependencies, for testing.
  static std::unique_ptr<IppClientInfoCalculator> CreateForTesting(
      std::unique_ptr<policy::DeviceAttributes> device_attributes,
      const std::string& chrome_milestone);
};

}  // namespace ash::printing

#endif  // CHROME_BROWSER_ASH_PRINTING_IPP_CLIENT_INFO_CALCULATOR_H_
