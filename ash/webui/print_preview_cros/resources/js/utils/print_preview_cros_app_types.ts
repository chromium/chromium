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

  // Whether orientation should be in landscape or portrait mode.
  landscape: boolean;

  // Whether to use predefined margins or custom.
  marginsType: MarginType;

  // Margins defined by users when marginsType is MarginType.CUSTOM_MARGINS.
  marginsCustom?: MarginsSetting;

  // Used to set requested media and printable area size.
  // See: printing/print_settings_conversion.cc
  mediaSize: MediaSize;
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
