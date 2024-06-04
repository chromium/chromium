// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ColorModel, DuplexMode, MarginType, MediaSize, PrinterStatusReason, type PrintTicket, ScalingType} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'ticket_constants' contains data structures and other constants related
 * to the print and preview ticket used in multiple files.
 */

// Default based on unavailable value for mediaSize.
// See: chrome/browser/resources/print_preview/data/model.ts.
const DEFAULT_MEDIA_SIZE: MediaSize = {
  widthMicrons: 215900,
  heightMicrons: 279400,
  imageableAreaLeftMicrons: 0,
  imageableAreaBottomMicrons: 0,
  imageableAreaRightMicrons: 215900,
  imageableAreaTopMicrons: 279400,
  hasBorderlessVariant: false,
};

// Default based on settings defaults described in createSettings function.
// See: chrome/browser/resources/print_preview/data/model.ts.
export const DEFAULT_PARTIAL_PRINT_TICKET: Partial<PrintTicket> = {
  borderless: false,
  collate: true,
  color: ColorModel.COLOR,
  copies: 1,
  dpiHorizontal: 0,
  dpiVertical: 0,
  dpiDefault: false,
  duplex: DuplexMode.LONG_EDGE,
  headerFooterEnabled: true,
  landscape: false,
  marginsType: MarginType.DEFAULT_MARGINS,
  mediaSize: DEFAULT_MEDIA_SIZE,
  mediaType: '',
  pageCount: [1],
  pagesPerSheet: 1,
  pageHeight: 0,
  pageWidth: 0,
  printerManuallySelected: false,
  printerStatusReason: PrinterStatusReason.UNKNOWN_REASON,
  rasterizePDF: false,
  scaleFactor: 100,
  scalingType: ScalingType.DEFAULT,
  shouldPrintBackgrounds: false,
};
