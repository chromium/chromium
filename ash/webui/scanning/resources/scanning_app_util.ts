// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {ColorMode, FileType, PageSize, Scanner, SourceType} from './scanning.mojom-webui.js';

/**
 * Converts a ColorMode string to the corresponding enum
 * value.
 */
export function colorModeFromString(colorModeString: string): ColorMode {
  switch (colorModeString) {
    case ColorMode.kBlackAndWhite.toString():
      return ColorMode.kBlackAndWhite;
    case ColorMode.kGrayscale.toString():
      return ColorMode.kGrayscale;
    case ColorMode.kColor.toString():
      return ColorMode.kColor;
    default:
      assertNotReached();
      return ColorMode.kColor;
  }
}

/**
 * Converts a FileType string to the corresponding
 * enum value.
 */
export function fileTypeFromString(fileTypeString: string): FileType {
  switch (fileTypeString) {
    case FileType.kJpg.toString():
      return FileType.kJpg;
    case FileType.kPdf.toString():
      return FileType.kPdf;
    case FileType.kPng.toString():
      return FileType.kPng;
    default:
      assertNotReached();
      return FileType.kPdf;
  }
}

/**
 * Converts a ColorMode to a string that can be
 * displayed in the color mode dropdown.
 */
export function getColorModeString(mojoColorMode: ColorMode): string {
  switch (mojoColorMode) {
    case ColorMode.kBlackAndWhite:
      return loadTimeData.getString('blackAndWhiteOptionText');
    case ColorMode.kGrayscale:
      return loadTimeData.getString('grayscaleOptionText');
    case ColorMode.kColor:
      return loadTimeData.getString('colorOptionText');
    default:
      assertNotReached();
      return loadTimeData.getString('blackAndWhiteOptionText');
  }
}

/**
 * Converts a PageSize to a string that can be
 * displayed in the page size dropdown.
 */
export function getPageSizeString(pageSize: PageSize): string {
  switch (pageSize) {
    case PageSize.kIsoA3:
      return loadTimeData.getString('a3OptionText');
    case PageSize.kIsoA4:
      return loadTimeData.getString('a4OptionText');
    case PageSize.kIsoB4:
      return loadTimeData.getString('b4OptionText');
    case PageSize.kLegal:
      return loadTimeData.getString('legalOptionText');
    case PageSize.kNaLetter:
      return loadTimeData.getString('letterOptionText');
    case PageSize.kTabloid:
      return loadTimeData.getString('tabloidOptionText');
    case PageSize.kMax:
      return loadTimeData.getString('fitToScanAreaOptionText');
    default:
      assertNotReached();
      return loadTimeData.getString('letterOptionText');
  }
}

/**
 * Converts a SourceType to a string that can be
 * displayed in the source dropdown.
 */
export function getSourceTypeString(mojoSourceType: SourceType): string {
  switch (mojoSourceType) {
    case SourceType.kFlatbed:
      return loadTimeData.getString('flatbedOptionText');
    case SourceType.kAdfSimplex:
      return loadTimeData.getString('oneSidedDocFeederOptionText');
    case SourceType.kAdfDuplex:
      return loadTimeData.getString('twoSidedDocFeederOptionText');
    case SourceType.kDefault:
      return loadTimeData.getString('defaultSourceOptionText');
    case SourceType.kUnknown:
    default:
      assertNotReached();
      return loadTimeData.getString('defaultSourceOptionText');
  }
}

/**
 * Converts a PageSize string to the corresponding enum
 * value.
 */
export function pageSizeFromString(pageSizeString: string): PageSize {
  switch (pageSizeString) {
    case PageSize.kIsoA3.toString():
      return PageSize.kIsoA3;
    case PageSize.kIsoA4.toString():
      return PageSize.kIsoA4;
    case PageSize.kIsoB4.toString():
      return PageSize.kIsoB4;
    case PageSize.kLegal.toString():
      return PageSize.kLegal;
    case PageSize.kNaLetter.toString():
      return PageSize.kNaLetter;
    case PageSize.kTabloid.toString():
      return PageSize.kTabloid;
    case PageSize.kMax.toString():
      return PageSize.kMax;
    default:
      assertNotReached();
      return PageSize.kNaLetter;
  }
}

/**
 * Converts a scanner's display name from UTF-16 to a displayable string.
 */
export function getScannerDisplayName(scanner: Scanner): string {
  return scanner.displayName.data.map(ch => String.fromCodePoint(ch)).join('');
}

/**
 * Converts an unguessable token to a string by combining the high and low
 * values as strings with a hashtag as the separator.
 */
export function tokenToString(token: UnguessableToken): string {
  return `${token.high.toString()}#${token.low.toString()}`;
}

/**
 * A comparison function used for determining sort order based on the current
 * locale's collation order.
 */
export function alphabeticalCompare(first: string, second: string): number {
  return first.toLocaleLowerCase().localeCompare(second.toLocaleLowerCase());
}
