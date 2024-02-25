// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/ppd_resolution_state.h"

#include "base/check.h"

namespace ash {

PpdResolutionState::PpdResolutionState()
    : is_inflight_(true), is_ppd_resolution_successful_(false) {}
PpdResolutionState::PpdResolutionState(PpdResolutionState&& other) = default;
PpdResolutionState& PpdResolutionState::operator=(PpdResolutionState&& rhs) =
    default;
PpdResolutionState::~PpdResolutionState() = default;

void PpdResolutionState::MarkResolutionSuccessful(
    const chromeos::Printer::PpdReference& ppd_reference) {
  DCHECK(is_inflight_);

  ppd_reference_ = ppd_reference;
  is_inflight_ = false;
  is_ppd_resolution_successful_ = true;
}

void PpdResolutionState::MarkResolutionFailed() {
  DCHECK(is_inflight_);

  is_inflight_ = false;
  is_ppd_resolution_successful_ = false;
}

void PpdResolutionState::SetUsbManufacturer(
    const std::string& usb_manufacturer) {
  DCHECK(!is_inflight_);
  DCHECK(!is_ppd_resolution_successful_);

  usb_manufacturer_ = usb_manufacturer;
}

const chromeos::Printer::PpdReference& PpdResolutionState::GetPpdReference()
    const {
  DCHECK(!is_inflight_);
  DCHECK(is_ppd_resolution_successful_);
  return ppd_reference_;
}

const std::string& PpdResolutionState::GetUsbManufacturer() const {
  DCHECK(!is_inflight_);
  return usb_manufacturer_;
}

bool PpdResolutionState::IsInflight() const {
  return is_inflight_;
}

bool PpdResolutionState::WasResolutionSuccessful() const {
  return is_ppd_resolution_successful_;
}

}  // namespace ash
