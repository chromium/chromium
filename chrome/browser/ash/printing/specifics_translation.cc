// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/specifics_translation.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/printing/printer_configuration.h"

namespace ash {

namespace {

chromeos::Printer::PpdReference SpecificsToPpd(
    const sync_pb::PrinterPPDReference& specifics) {
  chromeos::Printer::PpdReference ref;
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
                               const chromeos::Printer::PpdReference& ref) {
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

std::unique_ptr<chromeos::Printer> SpecificsToPrinter(
    const sync_pb::PrinterSpecifics& specifics) {
  DCHECK(!specifics.id().empty());

  auto printer = std::make_unique<chromeos::Printer>(specifics.id());
  printer->set_display_name(specifics.display_name());
  printer->set_description(specifics.description());
  if (!specifics.make_and_model().empty()) {
    printer->set_make_and_model(specifics.make_and_model());
  } else {
    printer->set_make_and_model(
        MakeAndModel(specifics.manufacturer(), specifics.model()));
  }

  bool result = false;
  std::string message;
  chromeos::Uri uri(specifics.uri());
  const chromeos::Uri::ParserStatus uri_error_code =
      uri.GetLastParsingError().status;
  if (uri_error_code == chromeos::Uri::ParserStatus::kNoErrors) {
    // Versions of Chrome <= R85 saved incorrectly AppSocket printers with a
    // default IPP path. Here, we have to make sure that URIs of these types of
    // printers do not contain a path component. It would cause an error in the
    // printer->SetUri(...) method.
    if (uri.GetScheme() == "socket")
      uri.SetPathEncoded("");
    result = printer->SetUri(uri, &message);
  } else {
    message = "Malformed URI, error code: " +
              base::NumberToString(static_cast<int>(uri_error_code));
  }
  if (!result)
    LOG(WARNING) << message;

  printer->set_uuid(specifics.uuid());
  printer->set_print_server_uri(specifics.print_server_uri());

  *printer->mutable_ppd_reference() = SpecificsToPpd(specifics.ppd_reference());

  return printer;
}

std::unique_ptr<sync_pb::PrinterSpecifics> PrinterToSpecifics(
    const chromeos::Printer& printer) {
  DCHECK(!printer.id().empty());

  auto specifics = std::make_unique<sync_pb::PrinterSpecifics>();
  specifics->set_id(printer.id());
  MergePrinterToSpecifics(printer, specifics.get());
  return specifics;
}

void MergePrinterToSpecifics(const chromeos::Printer& printer,
                             sync_pb::PrinterSpecifics* specifics) {
  // Never update id it needs to be stable.
  DCHECK_EQ(printer.id(), specifics->id());

  if (!printer.display_name().empty())
    specifics->set_display_name(printer.display_name());

  if (!printer.description().empty())
    specifics->set_description(printer.description());

  if (!printer.make_and_model().empty())
    specifics->set_make_and_model(printer.make_and_model());

  if (printer.HasUri())
    specifics->set_uri(printer.uri().GetNormalized());

  if (!printer.uuid().empty())
    specifics->set_uuid(printer.uuid());

  if (!printer.print_server_uri().empty())
    specifics->set_print_server_uri(printer.print_server_uri());

  MergeReferenceToSpecifics(specifics->mutable_ppd_reference(),
                            printer.ppd_reference());
}

std::string MakeAndModel(std::string_view make, std::string_view model) {
  return base::StartsWith(model, make) ? std::string(model)
                                       : base::JoinString({make, model}, " ");
}

}  // namespace ash
