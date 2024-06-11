// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {ColorModel, DuplexMode, MarginType, MediaSize, PageRange, PreviewTicket, PrinterType, ScalingType} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'fake_data' contains fake data to be used for tests and mocks.
 */

export function getFakePreviewTicket(requestId: number = 1): PreviewTicket {
  const previewTicket: PreviewTicket = {
    requestId: requestId,
    printPreviewId: new UnguessableToken(),
    destinationId: 'Default Printer',
    collate: true,
    color: ColorModel.COLOR,
    copies: 1,
    dpiHorizontal: 100,
    dpiVertical: 100,
    duplex: DuplexMode.SIMPLEX,
    headerFooterEnabled: true,
    landscape: false,
    marginsType: MarginType.DEFAULT_MARGINS,
    mediaSize: {
      heightMicrons: 279400,
      widthMicrons: 215900,
    } as MediaSize,
    pagesPerSheet: 1,
    previewModifiable: true,
    printerType: PrinterType.LOCAL_PRINTER,
    rasterizePDF: false,
    scaleFactor: 100,
    scalingType: ScalingType.DEFAULT,
    shouldPrintBackgrounds: false,
    shouldPrintSelectionOnly: false,
    pageRange: [{from: 1, to: 2} as PageRange],
    isFirstRequest: true,
  };

  return previewTicket;
}
