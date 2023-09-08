// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PPD_RESOLUTION_STATE_H_
#define CHROME_BROWSER_ASH_PRINTING_PPD_RESOLUTION_STATE_H_

#include <string>

#include "chromeos/printing/printer_configuration.h"

namespace ash {

class PpdResolutionState {
 public:
  PpdResolutionState();
  PpdResolutionState(PpdResolutionState&& other);
  PpdResolutionState& operator=(PpdResolutionState&& rhs);

  PpdResolutionState(const PpdResolutionState&) = delete;
  PpdResolutionState& operator=(const PpdResolutionState&) = delete;

  ~PpdResolutionState();

  // Marks PPD resolution was successful and stores |ppd_reference|.
  void MarkResolutionSuccessful(
      const chromeos::Printer::PpdReference& ppd_reference);

  // Marks PPD resolution was unsuccessful.
  void MarkResolutionFailed();

  // Store |usb_manufacturer|.
  void SetUsbManufacturer(const std::string& usb_manufacturer);

  // Getter function for |ppd_reference_|.
  const chromeos::Printer::PpdReference& GetPpdReference() const;

  // Getter function for |usb_manufacturer_|.
  const std::string& GetUsbManufacturer() const;

  // Returns true if the PPD resolution is inflight.
  bool IsInflight() const;

  // Returns true if a PpdReference was retrieved.
  bool WasResolutionSuccessful() const;

 private:
  bool is_inflight_;
  bool is_ppd_resolution_successful_;
  chromeos::Printer::PpdReference ppd_reference_;
  std::string usb_manufacturer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PPD_RESOLUTION_STATE_H_
