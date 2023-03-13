// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_test_utils.h"

#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"

namespace printing {

const char kDummyPrinterName[] = "DefaultPrinter";

const std::vector<gfx::Size> kTestPrinterCapabilitiesDefaultDpis{
    kTestPrinterCapabilitiesDpi};
const PrinterBasicInfoOptions kTestDummyPrintInfoOptions{{"opt1", "123"},
                                                         {"opt2", "456"}};

base::Value::Dict GetPrintTicket(mojom::PrinterType type) {
  DCHECK_NE(type, mojom::PrinterType::kPrivetDeprecated);

  base::Value::Dict ticket;

  // Letter
  base::Value::Dict media_size;
  media_size.Set(kSettingMediaSizeIsDefault, true);
  media_size.Set(kSettingMediaSizeWidthMicrons, 215900);
  media_size.Set(kSettingMediaSizeHeightMicrons, 279400);
  ticket.Set(kSettingMediaSize, std::move(media_size));

  ticket.Set(kSettingPreviewPageCount, 1);
  ticket.Set(kSettingLandscape, false);
  ticket.Set(kSettingColor, 2);  // color printing
  ticket.Set(kSettingHeaderFooterEnabled, false);
  ticket.Set(kSettingMarginsType, 0);  // default margins
  ticket.Set(kSettingDuplexMode,
             static_cast<int>(mojom::DuplexMode::kLongEdge));
  ticket.Set(kSettingCopies, 1);
  ticket.Set(kSettingCollate, true);
  ticket.Set(kSettingShouldPrintBackgrounds, false);
  ticket.Set(kSettingShouldPrintSelectionOnly, false);
  ticket.Set(kSettingPreviewModifiable, true);
  ticket.Set(kSettingPrinterType, static_cast<int>(type));
  ticket.Set(kSettingRasterizePdf, false);
  ticket.Set(kSettingScaleFactor, 100);
  ticket.Set(kSettingScalingType, FIT_TO_PAGE);
  ticket.Set(kSettingPagesPerSheet, 1);
  ticket.Set(kSettingDpiHorizontal, kTestPrinterDpi);
  ticket.Set(kSettingDpiVertical, kTestPrinterDpi);
  ticket.Set(kSettingDeviceName, kDummyPrinterName);
  ticket.Set(kSettingPageWidth, 215900);
  ticket.Set(kSettingPageHeight, 279400);
  ticket.Set(kSettingShowSystemDialog, false);

  if (type == mojom::PrinterType::kExtension) {
    base::Value::Dict capabilities;
    capabilities.Set("duplex", true);  // non-empty
    std::string caps_string;
    base::JSONWriter::Write(capabilities, &caps_string);
    ticket.Set(kSettingCapabilities, caps_string);
    base::Value::Dict print_ticket;
    print_ticket.Set("version", "1.0");
    print_ticket.Set("print", base::Value());
    std::string ticket_string;
    base::JSONWriter::Write(print_ticket, &ticket_string);
    ticket.Set(kSettingTicket, ticket_string);
  }

  return ticket;
}

std::unique_ptr<PrintSettings> MakeDefaultPrintSettings(
    const std::string& printer_name) {
  // Setup a sample page setup, which is needed to pass checks in
  // `PrintRenderFrameHelper` that the print params are valid.
  constexpr gfx::Size kPhysicalSize = gfx::Size(200, 200);
  constexpr gfx::Rect kPrintableArea = gfx::Rect(0, 0, 200, 200);
  const PageMargins kRequestedMargins(0, 0, 5, 5, 5, 5);
  const PageSetup kPageSetup(kPhysicalSize, kPrintableArea, kRequestedMargins,
                             /*forced_margins=*/false,
                             /*text_height=*/0);

  auto settings = std::make_unique<PrintSettings>();
  settings->set_copies(kTestPrintSettingsCopies);
  settings->set_dpi(kTestPrinterDefaultRenderDpi);
  settings->set_page_setup_device_units(kPageSetup);
  settings->set_device_name(base::ASCIIToUTF16(printer_name));
  return settings;
}

std::unique_ptr<PrintSettings> MakeUserModifiedPrintSettings(
    const std::string& printer_name) {
  std::unique_ptr<PrintSettings> settings =
      MakeDefaultPrintSettings(printer_name);
  settings->set_copies(kTestPrintSettingsCopies + 1);
  return settings;
}

}  // namespace printing
