// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_mojom_traits.h"

#include <cups/ipp.h>

#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/mojom/print.mojom-shared.h"
#include "printing/units.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"

namespace mojo {

namespace {
// sides:
using blink::mojom::WebPrintingSides;
using printing::mojom::DuplexMode;

// multiple-document-handling:
using MultipleDocumentHandling =
    blink::mojom::WebPrintingMultipleDocumentHandling;

// orientation-requested:
using OrientationRequested = blink::mojom::WebPrintingOrientationRequested;

// print-color-mode:
using PrintColorMode = blink::mojom::WebPrintColorMode;
using printing::mojom::ColorModel;

// printer-state-reason:
using PrinterStatusReason = printing::PrinterStatus::PrinterReason::Reason;
using blink::mojom::WebPrinterStateReason;

// This is not typemapped via EnumTraits<> due to issues with handling `auto`
// PrintColorMode (which doesn't represent a color model and hence has to be
// processed separately).
// As for specializing a TypeConverter<> -- since this function is not exposed
// publicly, we'd like to avoid potential ODR violations if someone decides to
// implement a converter between these two types elsewhere.
ColorModel PrintColorModeToColorModel(PrintColorMode print_color_mode) {
  switch (print_color_mode) {
    case PrintColorMode::kColor:
      return ColorModel::kColorModeColor;
    case PrintColorMode::kMonochrome:
      return ColorModel::kColorModeMonochrome;
  }
}

bool InferRequestedMedia(
    blink::mojom::WebPrintJobTemplateAttributesDataView data,
    printing::PrintSettings& settings) {
  std::optional<std::string> media_source;
  if (!data.ReadMediaSource(&media_source)) {
    return false;
  }
  if (media_source) {
    settings.advanced_settings().emplace(printing::kIppMediaSource,
                                         *media_source);
  }
  blink::mojom::WebPrintingMediaCollectionRequestedDataView media_col;
  data.GetMediaColDataView(&media_col);
  if (media_col.is_null()) {
    // media-col is an optional field.
    return true;
  }
  gfx::Size media_size;
  if (!media_col.ReadMediaSize(&media_size)) {
    return false;
  }
  // The incoming size is specified in hundredths of millimeters (PWG units)
  // whereas the printing subsystem operates on microns.
  settings.set_requested_media(
      {.size_microns = {media_size.width() * printing::kMicronsPerPwgUnit,
                        media_size.height() * printing::kMicronsPerPwgUnit}});
  return true;
}

}  // namespace

// static
blink::mojom::WebPrintingSides
EnumTraits<WebPrintingSides, DuplexMode>::ToMojom(
    printing::mojom::DuplexMode input) {
  switch (input) {
    case DuplexMode::kSimplex:
      return WebPrintingSides::kOneSided;
    case DuplexMode::kLongEdge:
      return WebPrintingSides::kTwoSidedLongEdge;
    case DuplexMode::kShortEdge:
      return WebPrintingSides::kTwoSidedShortEdge;
    case DuplexMode::kUnknownDuplexMode:
      NOTREACHED();
  }
}

// static
bool EnumTraits<WebPrintingSides, DuplexMode>::FromMojom(WebPrintingSides input,
                                                         DuplexMode* output) {
  switch (input) {
    case WebPrintingSides::kOneSided:
      *output = DuplexMode::kSimplex;
      return true;
    case WebPrintingSides::kTwoSidedLongEdge:
      *output = DuplexMode::kLongEdge;
      return true;
    case WebPrintingSides::kTwoSidedShortEdge:
      *output = DuplexMode::kShortEdge;
      return true;
  }
}

// static
blink::mojom::WebPrinterState
EnumTraits<blink::mojom::WebPrinterState, ipp_pstate_t>::ToMojom(
    ipp_pstate_t printer_state) {
  switch (printer_state) {
    case IPP_PSTATE_IDLE:
      return blink::mojom::WebPrinterState::kIdle;
    case IPP_PSTATE_PROCESSING:
      return blink::mojom::WebPrinterState::kIdle;
    case IPP_PSTATE_STOPPED:
      return blink::mojom::WebPrinterState::kStopped;
  }
}

// static
WebPrinterStateReason
EnumTraits<WebPrinterStateReason, PrinterStatusReason>::ToMojom(
    PrinterStatusReason printer_status_reason) {
  switch (printer_status_reason) {
    case PrinterStatusReason::kNone:
      return WebPrinterStateReason::kNone;
    case PrinterStatusReason::kUnknownReason:
      return WebPrinterStateReason::kOther;
    case PrinterStatusReason::kConnectingToDevice:
      return WebPrinterStateReason::kConnectingToDevice;
    case PrinterStatusReason::kCoverOpen:
      return WebPrinterStateReason::kCoverOpen;
    case PrinterStatusReason::kDeveloperEmpty:
      return WebPrinterStateReason::kDeveloperEmpty;
    case PrinterStatusReason::kDeveloperLow:
      return WebPrinterStateReason::kDeveloperLow;
    case PrinterStatusReason::kDoorOpen:
      return WebPrinterStateReason::kDoorOpen;
    case PrinterStatusReason::kFuserOverTemp:
      return WebPrinterStateReason::kFuserOverTemp;
    case PrinterStatusReason::kFuserUnderTemp:
      return WebPrinterStateReason::kFuserUnderTemp;
    case PrinterStatusReason::kInputTrayMissing:
      return WebPrinterStateReason::kInputTrayMissing;
    case PrinterStatusReason::kInterlockOpen:
      return WebPrinterStateReason::kInterlockOpen;
    case PrinterStatusReason::kInterpreterResourceUnavailable:
      return WebPrinterStateReason::kInterpreterResourceUnavailable;
    case PrinterStatusReason::kMarkerSupplyEmpty:
      return WebPrinterStateReason::kMarkerSupplyEmpty;
    case PrinterStatusReason::kMarkerSupplyLow:
      return WebPrinterStateReason::kMarkerSupplyLow;
    case PrinterStatusReason::kMarkerWasteAlmostFull:
      return WebPrinterStateReason::kMarkerWasteAlmostFull;
    case PrinterStatusReason::kMarkerWasteFull:
      return WebPrinterStateReason::kMarkerWasteFull;
    case PrinterStatusReason::kMediaEmpty:
      return WebPrinterStateReason::kMediaEmpty;
    case PrinterStatusReason::kMediaJam:
      return WebPrinterStateReason::kMediaJam;
    case PrinterStatusReason::kMediaLow:
      return WebPrinterStateReason::kMediaLow;
    case PrinterStatusReason::kMediaNeeded:
      return WebPrinterStateReason::kMediaNeeded;
    case PrinterStatusReason::kMovingToPaused:
      return WebPrinterStateReason::kMovingToPaused;
    case PrinterStatusReason::kOpcLifeOver:
      return WebPrinterStateReason::kOpcLifeOver;
    case PrinterStatusReason::kOpcNearEol:
      return WebPrinterStateReason::kOpcNearEol;
    case PrinterStatusReason::kOutputAreaAlmostFull:
      return WebPrinterStateReason::kOutputAreaAlmostFull;
    case PrinterStatusReason::kOutputAreaFull:
      return WebPrinterStateReason::kOutputAreaFull;
    case PrinterStatusReason::kOutputTrayMissing:
      return WebPrinterStateReason::kOutputTrayMissing;
    case PrinterStatusReason::kPaused:
      return WebPrinterStateReason::kPaused;
    case PrinterStatusReason::kShutdown:
      return WebPrinterStateReason::kShutdown;
    case PrinterStatusReason::kSpoolAreaFull:
      return WebPrinterStateReason::kSpoolAreaFull;
    case PrinterStatusReason::kStoppedPartly:
      return WebPrinterStateReason::kStoppedPartly;
    case PrinterStatusReason::kStopping:
      return WebPrinterStateReason::kStopping;
    case PrinterStatusReason::kTimedOut:
      return WebPrinterStateReason::kTimedOut;
    case PrinterStatusReason::kTonerEmpty:
      return WebPrinterStateReason::kTonerEmpty;
    case PrinterStatusReason::kTonerLow:
      return WebPrinterStateReason::kTonerLow;
    case PrinterStatusReason::kCupsPkiExpired:
      return WebPrinterStateReason::kCupsPkiExpired;
  }
}

// static
bool StructTraits<blink::mojom::WebPrintJobTemplateAttributesDataView,
                  std::unique_ptr<printing::PrintSettings>>::
    Read(blink::mojom::WebPrintJobTemplateAttributesDataView data,
         std::unique_ptr<printing::PrintSettings>* out) {
  auto settings = std::make_unique<printing::PrintSettings>();

  settings->set_copies(data.copies());
  {
    std::string job_name;
    if (!data.ReadJobName(&job_name)) {
      return false;
    }
    settings->set_title(base::UTF8ToUTF16(job_name));
  }
  {
    std::optional<DuplexMode> duplex_mode;
    if (!data.ReadSides(&duplex_mode)) {
      return false;
    }
    if (duplex_mode) {
      settings->set_duplex_mode(*duplex_mode);
    }
  }
  if (auto orientation = data.orientation_requested()) {
    switch (*orientation) {
      case OrientationRequested::kPortrait:
        settings->SetOrientation(/*landscape=*/false);
        break;
      case OrientationRequested::kLandscape:
        settings->SetOrientation(/*landscape=*/true);
        break;
    }
  }
  if (!InferRequestedMedia(data, *settings)) {
    return false;
  }
  if (auto mdh = data.multiple_document_handling()) {
    switch (*mdh) {
      case MultipleDocumentHandling::kSeparateDocumentsCollatedCopies:
        settings->set_collate(true);
        break;
      case MultipleDocumentHandling::kSeparateDocumentsUncollatedCopies:
        settings->set_collate(false);
        break;
    }
  }
  {
    std::optional<gfx::Size> printer_resolution;
    if (!data.ReadPrinterResolution(&printer_resolution)) {
      return false;
    }
    if (printer_resolution) {
      settings->set_dpi_xy(printer_resolution->width(),
                           printer_resolution->height());
    }
  }
  if (auto print_color_mode = data.print_color_mode()) {
    settings->set_color(PrintColorModeToColorModel(*print_color_mode));
  }

  *out = std::move(settings);
  return true;
}

}  // namespace mojo
