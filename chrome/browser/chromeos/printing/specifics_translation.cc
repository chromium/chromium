// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/specifics_translation.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

namespace {

Printer::PpdReference SpecificsToPpd(
    const sync_pb::PrinterPPDReference& specifics) {
  Printer::PpdReference ref;
  if (specifics.autoconf()) {
    ref.autoconf = specifics.autoconf();
  } else if (specifics.has_user_supplied_ppd_url()) {
    ref.user_supplied_ppd_url = specifics.user_supplied_ppd_url();
  } else if (specifics.has_effective_make_and_model()) {
    ref.effective_make_and_model = specifics.effective_make_and_model();
  }

  return ref;
}

// Overwrite fields in |specifics| with an appropriately filled field from
// |ref|.  If |ref| is the default object, nothing will be changed in
// |specifics|.
void MergeReferenceToSpecifics(sync_pb::PrinterPPDReference* specifics,
                               const Printer::PpdReference& ref) {
  if (ref.autoconf) {
    specifics->Clear();
    specifics->set_autoconf(ref.autoconf);
  } else if (!ref.user_supplied_ppd_url.empty()) {
    specifics->Clear();
    specifics->set_user_supplied_ppd_url(ref.user_supplied_ppd_url);
  } else if (!ref.effective_make_and_model.empty()) {
    specifics->Clear();
    specifics->set_effective_make_and_model(ref.effective_make_and_model);
  }
}

}  // namespace

std::unique_ptr<Printer> SpecificsToPrinter(
    const sync_pb::PrinterSpecifics& specifics) {
  DCHECK(!specifics.id().empty());

  auto printer = std::make_unique<Printer>(specifics.id());
  printer->set_display_name(specifics.display_name());
  printer->set_description(specifics.description());
  printer->set_manufacturer(specifics.manufacturer());
  printer->set_model(specifics.model());
  if (!specifics.make_and_model().empty()) {
    printer->set_make_and_model(specifics.make_and_model());
  } else {
    printer->set_make_and_model(
        MakeAndModel(specifics.manufacturer(), specifics.model()));
  }
  printer->set_uri(specifics.uri());
  printer->set_uuid(specifics.uuid());

  *printer->mutable_ppd_reference() = SpecificsToPpd(specifics.ppd_reference());

  return printer;
}

std::unique_ptr<sync_pb::PrinterSpecifics> PrinterToSpecifics(
    const Printer& printer) {
  DCHECK(!printer.id().empty());

  auto specifics = std::make_unique<sync_pb::PrinterSpecifics>();
  specifics->set_id(printer.id());
  MergePrinterToSpecifics(printer, specifics.get());
  return specifics;
}

void MergePrinterToSpecifics(const Printer& printer,
                             sync_pb::PrinterSpecifics* specifics) {
  // Never update id it needs to be stable.
  DCHECK_EQ(printer.id(), specifics->id());

  if (!printer.display_name().empty())
    specifics->set_display_name(printer.display_name());

  if (!printer.description().empty())
    specifics->set_description(printer.description());

  if (!printer.manufacturer().empty())
    specifics->set_manufacturer(printer.manufacturer());

  if (!printer.model().empty())
    specifics->set_model(printer.model());

  if (!printer.make_and_model().empty())
    specifics->set_make_and_model(printer.make_and_model());

  if (!printer.uri().empty())
    specifics->set_uri(printer.uri());

  if (!printer.uuid().empty())
    specifics->set_uuid(printer.uuid());

  MergeReferenceToSpecifics(specifics->mutable_ppd_reference(),
                            printer.ppd_reference());
}

std::string MakeAndModel(base::StringPiece make, base::StringPiece model) {
  return model.starts_with(make) ? model.as_string()
                                 : base::JoinString({make, model}, " ");
}

}  // namespace chromeos
