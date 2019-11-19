// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/ppd_resolution_tracker.h"

#include "base/stl_util.h"
#include "chrome/browser/chromeos/printing/ppd_resolution_state.h"

namespace chromeos {

PpdResolutionTracker::PpdResolutionTracker() = default;
PpdResolutionTracker::PpdResolutionTracker(PpdResolutionTracker&& other) =
    default;
PpdResolutionTracker& PpdResolutionTracker::operator=(
    PpdResolutionTracker&& rhs) = default;
PpdResolutionTracker::~PpdResolutionTracker() = default;

bool PpdResolutionTracker::IsResolutionComplete(
    const std::string& printer_id) const {
  if (PrinterStateExists(printer_id)) {
    return !printer_state_.at(printer_id).IsInflight();
  }
  return false;
}

bool PpdResolutionTracker::IsResolutionPending(
    const std::string& printer_id) const {
  if (PrinterStateExists(printer_id)) {
    return printer_state_.at(printer_id).IsInflight();
  }
  return false;
}

bool PpdResolutionTracker::WasResolutionSuccessful(
    const std::string& printer_id) const {
  DCHECK(PrinterStateExists(printer_id));

  return printer_state_.at(printer_id).WasResolutionSuccessful();
}

void PpdResolutionTracker::MarkResolutionPending(
    const std::string& printer_id) {
  DCHECK(!PrinterStateExists(printer_id));

  // Default state of PpdResolution is when resolution is inflight.
  printer_state_[printer_id] = PpdResolutionState();
}

void PpdResolutionTracker::MarkResolutionSuccessful(
    const std::string& printer_id,
    const Printer::PpdReference& ppd_reference) {
  DCHECK(PrinterStateExists(printer_id));
  DCHECK(IsResolutionPending(printer_id));

  printer_state_.at(printer_id).MarkResolutionSuccessful(ppd_reference);
}

void PpdResolutionTracker::MarkResolutionFailed(const std::string& printer_id) {
  DCHECK(PrinterStateExists(printer_id));
  DCHECK(IsResolutionPending(printer_id));

  printer_state_.at(printer_id).MarkResolutionFailed();
}

void PpdResolutionTracker::SetManufacturer(
    const std::string& printer_id,
    const std::string& usb_manufacturer) {
  DCHECK(PrinterStateExists(printer_id));

  printer_state_.at(printer_id).SetUsbManufacturer(usb_manufacturer);
}

const std::string& PpdResolutionTracker::GetManufacturer(
    const std::string& printer_id) const {
  DCHECK(PrinterStateExists(printer_id));

  return printer_state_.at(printer_id).GetUsbManufacturer();
}

const Printer::PpdReference& PpdResolutionTracker::GetPpdReference(
    const std::string& printer_id) const {
  DCHECK(PrinterStateExists(printer_id));

  return printer_state_.at(printer_id).GetPpdReference();
}

bool PpdResolutionTracker::PrinterStateExists(
    const std::string& printer_id) const {
  return base::Contains(printer_state_, printer_id);
}

}  // namespace chromeos
