// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_SPECIFICS_TRANSLATION_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_SPECIFICS_TRANSLATION_H_

#include <memory>

#include "base/strings/string_piece.h"
#include "components/sync/protocol/printer_specifics.pb.h"

namespace chromeos {

class Printer;

// Convert |printer| into its local representation.  Enforces that only one
// field in PpdReference is filled in.  In order of preference, we populate
// autoconf, user_supplied_ppd_url, or effective_make_and_model.
std::unique_ptr<Printer> SpecificsToPrinter(
    const sync_pb::PrinterSpecifics& printer);

// Convert |printer| into its proto representation.
std::unique_ptr<sync_pb::PrinterSpecifics> PrinterToSpecifics(
    const Printer& printer);

// Merge fields from |printer| into |specifics|.  Merge strategy is to only
// write non-default fields from |printer| into the appropriate field in
// |specifics|.  Default fields are skipped to prevent accidentally clearing
// |specifics|.  Enforces field exclusivity in PpdReference as described in
// SpecificsToPrinter.
void MergePrinterToSpecifics(const Printer& printer,
                             sync_pb::PrinterSpecifics* specifics);

// Combines |make| and |model| with a space to generate a make and model string.
// If |model| already represents the make and model, the string is just |model|.
// This is to prevent strings of the form '<make> <make> <model>'.
std::string MakeAndModel(base::StringPiece make, base::StringPiece model);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_SPECIFICS_TRANSLATION_H_
