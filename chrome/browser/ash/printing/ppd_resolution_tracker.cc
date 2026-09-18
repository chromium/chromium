// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/ppd_resolution_tracker.h"

#include "chrome/browser/ash/printing/ppd_resolution_state.h"

namespace ash {

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
  CHECK(PrinterStateExists(printer_id), base::NotFatalUntil::M160);

  return printer_state_.at(printer_id).WasResolutionSuccessful();
}

void PpdResolutionTracker::MarkResolutionPending(
    const std::string& printer_id) {
  CHECK(!PrinterStateExists(printer_id), base::NotFatalUntil::M160);

  // Default state of PpdResolution is when resolution is inflight.
  printer_state_[printer_id] = PpdResolutionState();
}

void PpdResolutionTracker::MarkResolutionSuccessful(
    const std::string& printer_id,
    const chromeos::Printer::PpdReference& ppd_reference) {
  CHECK(PrinterStateExists(printer_id), base::NotFatalUntil::M160);
  CHECK(IsResolutionPending(printer_id), base::NotFatalUntil::M160);

  printer_state_.at(printer_id).MarkResolutionSuccessful(ppd_reference);
}

void PpdResolutionTracker::MarkResolutionFailed(const std::string& printer_id) {
  CHECK(PrinterStateExists(printer_id), base::NotFatalUntil::M160);
  CHECK(IsResolutionPending(printer_id), base::NotFatalUntil::M160);

  printer_state_.at(printer_id).MarkResolutionFailed();
}

void PpdResolutionTracker::SetManufacturer(
    const std::string& printer_id,
    const std::string& usb_manufacturer) {
  CHECK(PrinterStateExists(printer_id), base::NotFatalUntil::M160);

  printer_state_.at(printer_id).SetUsbManufacturer(usb_manufacturer);
}

const std::string& PpdResolutionTracker::GetManufacturer(
    const std::string& printer_id) const {
  CHECK(PrinterStateExists(printer_id), base::NotFatalUntil::M160);

  return printer_state_.at(printer_id).GetUsbManufacturer();
}

const chromeos::Printer::PpdReference& PpdResolutionTracker::GetPpdReference(
    const std::string& printer_id) const {
  CHECK(PrinterStateExists(printer_id), base::NotFatalUntil::M160);

  return printer_state_.at(printer_id).GetPpdReference();
}

bool PpdResolutionTracker::PrinterStateExists(
    const std::string& printer_id) const {
  return printer_state_.contains(printer_id);
}

}  // namespace ash
