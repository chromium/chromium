// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

/**
 * @fileoverview
 * 'print_preview_cros_app_types' contains app specific and mojo placeholder
 * types.
 */

// Common data for displaying and filtering print destinations.
export interface Destination {
  // ID can be the printer name or ID depending on the originating type of
  // printer.
  id: string;

  // Display name from printer.
  displayName: string;

  // Type of destination.
  printerType: PrinterType;

  // Used for metrics, true when destination manually selected during CrOS
  // preview session.
  printerManuallySelected: boolean;

  // The printer status reason for a local Chrome OS printer.
  printerStatusReason: PrinterStatusReason|null;
}

export interface PrintRequestOutcome {
  success: boolean;
  error?: string;
}

// Based on printing::mojom::ColorModel.
export enum ColorModel {
  UNKNOWN_COLOR_MODEL = 0,
  GRAY = 1,
  COLOR = 2,
}

// Constant values matching printing::DuplexMode enum.
export enum DuplexMode {
  SIMPLEX = 0,
  LONG_EDGE = 1,
  SHORT_EDGE = 2,
  UNKNOWN_DUPLEX_MODE = -1,
}

// Constant values matching printing::mojom::MarginType enum.
export enum MarginType {
  // Default varies depending on headers being enabled or not
  DEFAULT_MARGINS = 0,
  NO_MARGINS = 1,
  PRINTABLE_AREA_MARGINS = 2,
  CUSTOM_MARGINS = 3,
}

/**
 * Keep in sync with the C++ kSettingMargin... values in
 * printing/print_job_constants.h. Numbers are stored as integer values as that
 * is what printing::PageMargins class expects.
 */
export interface MarginsSetting {
  marginTop: number;
  marginRight: number;
  marginBottom: number;
  marginLeft: number;
}

export interface MediaSize {
  widthMicrons: number;
  heightMicrons: number;
  imageableAreaLeftMicrons?: number;
  imageableAreaBottomMicrons?: number;
  imageableAreaRightMicrons?: number;
  imageableAreaTopMicrons?: number;
  hasBorderlessVariant?: boolean;
}

// Chrome preview only allows the following options for pagesPerSheet.
// TODO(b/323421684): Revisit allowed values for pages-per-sheet after
// understanding the expectations of the n-up service/engine.
export type PagesPerSheetValue = 1|2|4|6|9|16;

/**
 * Printer types for capabilities and printer list requests.
 * Must match PrinterType in printing/mojom/print.mojom
 */
export enum PrinterType {
  PRIVET_PRINTER_DEPRECATED = 0,
  EXTENSION_PRINTER = 1,
  PDF_PRINTER = 2,
  LOCAL_PRINTER = 3,
  CLOUD_PRINTER_DEPRECATED = 4
}

/**
 * Must be kept in sync with the C++ ScalingType enum in
 * printing/print_job_constants.h.
 */
export enum ScalingType {
  DEFAULT = 0,
  FIT_TO_PAGE = 1,
  FIT_TO_PAPER = 2,
  CUSTOM = 3,
}

/**
 *  These values must be kept in sync with the Reason enum in
 *  /chromeos/printing/cups_printer_status.h
 */
export enum PrinterStatusReason {
  UNKNOWN_REASON = 0,
  DEVICE_ERROR = 1,
  DOOR_OPEN = 2,
  LOW_ON_INK = 3,
  LOW_ON_PAPER = 4,
  NO_ERROR = 5,
  OUT_OF_INK = 6,
  OUT_OF_PAPER = 7,
  OUTPUT_ALMOST_FULL = 8,
  OUTPUT_FULL = 9,
  PAPER_JAM = 10,
  PAUSED = 11,
  PRINTER_QUEUE_FULL = 12,
  PRINTER_UNREACHABLE = 13,
  STOPPED = 14,
  TRAY_MISSING = 15,
}

// PrintTicket represents the data required to start print job. Ticket will be
// used to create a settings dictionary with fields matching the existing Chrome
// preview print settings for reusability.
// TODO(b/323421684): Add missing required settings to start print job.
export interface PrintTicket {
  // ID used to map a CrOS preview session to the responsible PrintViewManager
  // and related web contents.
  printPreviewId: UnguessableToken;

  // ID of the destination print job will be sent to.
  destination: string;

  // Whether source document is PDF or HTML.
  previewModifiable: boolean;

  // Whether to print full document or selected section.
  shouldPrintSelectionOnly: boolean;

  // Additional vendor/advance job configuration such as 'job-sheet'.
  advancedSettings?: Map<string, any>;

  // Whether media should use borderless variant.
  borderless: boolean;

  // Used when printing multiple copies. When true, prints a full set of the
  // document before printing the next copy. When false, prints N-copies of page
  // one, then page two until all pages are printed.
  collate: boolean;

  // Print job color mode value.
  color: ColorModel;

  // Number of prints to make of source document.
  copies: number;

  // Horizontal DPI used for setting print job resolution.
  dpiHorizontal: number;

  // Vertical DPI used for setting print job resolution.
  dpiVertical: number;

  // Whether DPI used is the destination's default DPI.
  dpiDefault: boolean;

  // Determine if printing should be done on both sides and along which edge
  // of the media.
  duplex: DuplexMode;

  // Whether the header and footer content will be added to generated PDF.
  headerFooterEnabled: boolean;

  // Whether orientation should be in landscape or portrait mode.
  landscape: boolean;

  // Whether to use predefined margins or custom.
  marginsType: MarginType;

  // Margins defined by users when marginsType is MarginType.CUSTOM_MARGINS.
  marginsCustom?: MarginsSetting;

  // Used to set requested media and printable area size.
  // See: printing/print_settings_conversion.cc
  mediaSize: MediaSize;

  // Vendor id for a media type (plain paper, photo paper, etc.). For example,
  // vendor id ‘glossygold’ for media type Photo Paper Plus Glossy II.
  mediaType: string;

  // Number of pages/sheets in generated PDF. Value takes into account n-up.
  pageCount: number[];

  // For n-up, number of pages to print on a single sheet.
  pagesPerSheet: PagesPerSheetValue;

  // Height of page from generated PDF summing content, top margin, and bottom
  // margin.
  pageHeight: number;

  // Width of page from generated PDF summing content, left margin, and right
  // margin.
  pageWidth: number;

  // String containing a four digit numeric code when set.
  pinValue?: string;

  // Used for metrics. True when user updates the destination in the preview UI;
  // otherwise false.
  printerManuallySelected: boolean;

  // Used for metrics. Destination's PrinterStatus when launching print job.
  printerStatusReason: PrinterStatusReason;

  // Printer type used determine correct logic and handler for print job
  // destination.
  printerType: PrinterType;

  // Whether to treat source as an image when generating PDF.
  rasterizePDF: boolean;

  // Percent to scale source as integer.
  scaleFactor: number;

  // Whether to use custom scale or presets.
  scalingType: ScalingType;

  // Whether to generate PDF with CSS backgrounds included.
  shouldPrintBackgrounds: boolean;
}

// Immutable session configuration details for the current CrOS preview request.
export interface SessionContext {
  // ID used to map a CrOS preview session to the responsible PrintViewManager
  // and related web contents.
  printPreviewId: UnguessableToken;

  // Print param which tells whether source is a PDF or HTML(modifiable).
  isModifiable: boolean;

  // Print param which tells whether source is has a selected section to
  // determine if option to print only that selection should be provided.
  hasSelection: boolean;
}

// Placeholder for PrintPreviewPageHandler mojo interface.
export interface PrintPreviewPageHandler {
  // Completes initialization on the backend and provides immutable
  // configuration details for the current CrOS preview request in the form of
  // a SessionContext.
  startSession(): Promise<SessionContext>;

  // Start the print job and close the window. Requires a print ticket to detail
  // how print job should be configured.Needs to wait for result to display
  // error messaging if starting the print job fails.
  print(ticket: PrintTicket): Promise<PrintRequestOutcome>;

  // Cancel the print preview and close the window.
  cancel(): void;
}

// Placeholder for the DestinationProvider mojo interface.
export interface DestinationProvider {
  // Retrieve a list of local print destinations; usually provided by CUPS.
  getLocalDestinations(): Promise<Destination[]>;
}
