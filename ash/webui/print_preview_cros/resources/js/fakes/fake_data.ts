// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {Capabilities, CollateCapability, ColorCapability, ColorModel, ColorOption, CopiesCapability, DpiCapability, DpiOption, DuplexCapability, DuplexMode, DuplexOption, MarginType, MediaSize, MediaSizeCapability, MediaSizeOption, MediaTypeCapability, MediaTypeOption, PageOrientationCapability, PageOrientationOption, PageRange, PinCapability, PreviewTicket, PrinterType, ScalingType, SelectOption} from '../utils/print_preview_cros_app_types.js';

/**
 * @fileoverview
 * 'fake_data' contains fake data to be used for tests and mocks.
 */

export function getFakePreviewTicket(): PreviewTicket {
  const previewTicket: PreviewTicket = {
    requestId: 1,
    printPreviewId: new UnguessableToken(),
    deviceName: 'Default Printer',
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


export function getFakeCapabilities(destinationId: string = 'Printer1'):
    Capabilities {
  const collate: CollateCapability = {
    valueDefault: true,
  };

  const color: ColorCapability = {
    options: [
      {
        type: 'STANDARD_COLOR',
        vendorId: '1',
        isDefault: true,
      } as ColorOption,
      {
        type: 'STANDARD_MONOCHROME',
        vendorId: '2',
      } as ColorOption,
    ],
    resetToDefault: false,
  };

  const copies: CopiesCapability = {
    valueDefault: 1,
    max: 9999,
  };

  const duplex: DuplexCapability = {
    options: [
      {
        type: 'NO_DUPLEX',
        isDefault: true,
      } as DuplexOption,
      {
        type: 'LONG_EDGE',
      } as DuplexOption,
      {
        type: 'SHORT_EDGE',
      } as DuplexOption,
    ],
  };

  const pageOrientation: PageOrientationCapability = {
    options: [
      {
        type: 'PORTRAIT',
        isDefault: true,
      } as PageOrientationOption,
      {
        type: 'LANDSCAPE',
      } as PageOrientationOption,
      {type: 'AUTO'} as PageOrientationOption,
    ],
    resetToDefault: false,
  };

  const mediaSize: MediaSizeCapability = {
    options: [
      {
        vendorId: 'na_govt-letter_8x10in',
        heightMicrons: 254000,
        widthMicrons: 203200,
        imageableAreaLeftMicrons: 3000,
        imageableAreaBottomMicrons: 3000,
        imageableAreaRightMicrons: 200200,
        imageableAreaTopMicrons: 251000,
        hasBorderlessVariant: true,
        selectOption: {
          customDisplayName: '8 x 10 in',
          name: 'NA_GOVT_LETTER',
        } as SelectOption,
      } as MediaSizeOption,
      {
        vendorId: 'na_legal_8.5x14in',
        heightMicrons: 297000,
        widthMicrons: 210000,
        imageableAreaLeftMicrons: 3000,
        imageableAreaBottomMicrons: 3000,
        imageableAreaRightMicrons: 207000,
        imageableAreaTopMicrons: 294000,
        hasBorderlessVariant: true,
        selectOption: {
          customDisplayName: 'A4',
          name: 'ISO_A4',
        } as SelectOption,
      } as MediaSizeOption,
      {
        vendorId: 'na_legal_8.5x14in',
        heightMicrons: 355600,
        widthMicrons: 215900,
        imageableAreaLeftMicrons: 3000,
        imageableAreaBottomMicrons: 3000,
        imageableAreaRightMicrons: 212900,
        imageableAreaTopMicrons: 352600,
        selectOption: {
          customDisplayName: 'Legal',
          name: 'NA_LEGAL',
        } as SelectOption,
      } as MediaSizeOption,
      {
        vendorId: 'na_letter_8.5x11in',
        heightMicrons: 279400,
        widthMicrons: 215900,
        imageableAreaLeftMicrons: 3000,
        imageableAreaBottomMicrons: 3000,
        imageableAreaRightMicrons: 212900,
        imageableAreaTopMicrons: 276400,
        hasBorderlessVariant: true,
        selectOption: {
          customDisplayName: 'Letter',
          name: 'NA_LETTER',
        } as SelectOption,
      } as MediaSizeOption,
    ],
    resetToDefault: false,
  };

  const mediaType: MediaTypeCapability = {
    options: [
      {
        vendorId: 'stationery',
        resetToDefault: false,
        selectOption: {
          customDisplayName: 'Paper (Plain)',
          isDefault: true,
        } as SelectOption,
      } as MediaTypeOption,
      {
        vendorId: 'photographic',
        resetToDefault: false,
        selectOption: {
          customDisplayName: 'Photo',
          isDefault: true,
        } as SelectOption,
      } as MediaTypeOption,
      {
        vendorId: 'envelope',
        resetToDefault: false,
        selectOption: {
          customDisplayName: 'Envelope',
          isDefault: true,
        } as SelectOption,
      } as MediaTypeOption,
    ],
    resetToDefault: false,
  };

  const dpi: DpiCapability = {
    options: [
      {
        horizontalDpi: 300,
        verticalDpi: 300,
        isDefault: true,
      } as DpiOption,
      {
        horizontalDpi: 600,
        verticalDpi: 600,
      } as DpiOption,
      {
        horizontalDpi: 800,
        verticalDpi: 1000,
      } as DpiOption,
    ],
    resetToDefault: false,
  };

  const pin: PinCapability = {
    supported: false,
  };

  const capabilities: Capabilities = {
    destinationId: destinationId,
    collate: collate,
    color: color,
    copies: copies,
    duplex: duplex,
    pageOrientation: pageOrientation,
    mediaSize: mediaSize,
    mediaType: mediaType,
    dpi: dpi,
    pin: pin,
  };
  return capabilities;
}
