// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertNotReached} from 'chrome://resources/ash/common/assert.js';

/**
 * Converts a ash.scanning.mojom.ColorMode string to the corresponding enum
 * value.
 * @param {string} colorModeString
 * @return {ash.scanning.mojom.ColorMode}
 */
export function colorModeFromString(colorModeString) {
  switch (colorModeString) {
    case ash.scanning.mojom.ColorMode.kBlackAndWhite.toString():
      return ash.scanning.mojom.ColorMode.kBlackAndWhite;
    case ash.scanning.mojom.ColorMode.kGrayscale.toString():
      return ash.scanning.mojom.ColorMode.kGrayscale;
    case ash.scanning.mojom.ColorMode.kColor.toString():
      return ash.scanning.mojom.ColorMode.kColor;
    default:
      assertNotReached();
      return ash.scanning.mojom.ColorMode.kColor;
  }
}

/**
 * Converts a ash.scanning.mojom.FileType string to the corresponding
 * enum value.
 * @param {string} fileTypeString
 * @return {ash.scanning.mojom.FileType}
 */
export function fileTypeFromString(fileTypeString) {
  switch (fileTypeString) {
    case ash.scanning.mojom.FileType.kJpg.toString():
      return ash.scanning.mojom.FileType.kJpg;
    case ash.scanning.mojom.FileType.kPdf.toString():
      return ash.scanning.mojom.FileType.kPdf;
    case ash.scanning.mojom.FileType.kPng.toString():
      return ash.scanning.mojom.FileType.kPng;
    default:
      assertNotReached();
      return ash.scanning.mojom.FileType.kPdf;
  }
}

/**
 * Converts a ash.scanning.mojom.ColorMode to a string that can be
 * displayed in the color mode dropdown.
 * @param {ash.scanning.mojom.ColorMode} mojoColorMode
 * @return {string}
 */
export function getColorModeString(mojoColorMode) {
  switch (mojoColorMode) {
    case ash.scanning.mojom.ColorMode.kBlackAndWhite:
      return loadTimeData.getString('blackAndWhiteOptionText');
    case ash.scanning.mojom.ColorMode.kGrayscale:
      return loadTimeData.getString('grayscaleOptionText');
    case ash.scanning.mojom.ColorMode.kColor:
      return loadTimeData.getString('colorOptionText');
    default:
      assertNotReached();
      return loadTimeData.getString('blackAndWhiteOptionText');
  }
}

/**
 * Converts a ash.scanning.mojom.PageSize to a string that can be
 * displayed in the page size dropdown.
 * @param {ash.scanning.mojom.PageSize} pageSize
 * @return {string}
 */
export function getPageSizeString(pageSize) {
  switch (pageSize) {
    case ash.scanning.mojom.PageSize.kIsoA3:
      return loadTimeData.getString('a3OptionText');
    case ash.scanning.mojom.PageSize.kIsoA4:
      return loadTimeData.getString('a4OptionText');
    case ash.scanning.mojom.PageSize.kIsoB4:
      return loadTimeData.getString('b4OptionText');
    case ash.scanning.mojom.PageSize.kLegal:
      return loadTimeData.getString('legalOptionText');
    case ash.scanning.mojom.PageSize.kNaLetter:
      return loadTimeData.getString('letterOptionText');
    case ash.scanning.mojom.PageSize.kTabloid:
      return loadTimeData.getString('tabloidOptionText');
    case ash.scanning.mojom.PageSize.kMax:
      return loadTimeData.getString('fitToScanAreaOptionText');
    default:
      assertNotReached();
      return loadTimeData.getString('letterOptionText');
  }
}

/**
 * Converts a ash.scanning.mojom.SourceType to a string that can be
 * displayed in the source dropdown.
 * @param {ash.scanning.mojom.SourceType} mojoSourceType
 * @return {string}
 */
export function getSourceTypeString(mojoSourceType) {
  switch (mojoSourceType) {
    case ash.scanning.mojom.SourceType.kFlatbed:
      return loadTimeData.getString('flatbedOptionText');
    case ash.scanning.mojom.SourceType.kAdfSimplex:
      return loadTimeData.getString('oneSidedDocFeederOptionText');
    case ash.scanning.mojom.SourceType.kAdfDuplex:
      return loadTimeData.getString('twoSidedDocFeederOptionText');
    case ash.scanning.mojom.SourceType.kDefault:
      return loadTimeData.getString('defaultSourceOptionText');
    case ash.scanning.mojom.SourceType.kUnknown:
    default:
      assertNotReached();
      return loadTimeData.getString('defaultSourceOptionText');
  }
}

/**
 * Converts a ash.scanning.mojom.PageSize string to the corresponding enum
 * value.
 * @param {string} pageSizeString
 * @return {ash.scanning.mojom.PageSize}
 */
export function pageSizeFromString(pageSizeString) {
  switch (pageSizeString) {
    case ash.scanning.mojom.PageSize.kIsoA3.toString():
      return ash.scanning.mojom.PageSize.kIsoA3;
    case ash.scanning.mojom.PageSize.kIsoA4.toString():
      return ash.scanning.mojom.PageSize.kIsoA4;
    case ash.scanning.mojom.PageSize.kIsoB4.toString():
      return ash.scanning.mojom.PageSize.kIsoB4;
    case ash.scanning.mojom.PageSize.kLegal.toString():
      return ash.scanning.mojom.PageSize.kLegal;
    case ash.scanning.mojom.PageSize.kNaLetter.toString():
      return ash.scanning.mojom.PageSize.kNaLetter;
    case ash.scanning.mojom.PageSize.kTabloid.toString():
      return ash.scanning.mojom.PageSize.kTabloid;
    case ash.scanning.mojom.PageSize.kMax.toString():
      return ash.scanning.mojom.PageSize.kMax;
    default:
      assertNotReached();
      return ash.scanning.mojom.PageSize.kNaLetter;
  }
}

/**
 * Converts a scanner's display name from UTF-16 to a displayable string.
 * @param {!ash.scanning.mojom.Scanner} scanner
 * @return {string}
 */
export function getScannerDisplayName(scanner) {
  return scanner.displayName.data.map(ch => String.fromCodePoint(ch)).join('');
}

/**
 * Converts an unguessable token to a string by combining the high and low
 * values as strings with a hashtag as the separator.
 * @param {!mojoBase.mojom.UnguessableToken} token
 * @return {string}
 */
export function tokenToString(token) {
  return `${token.high.toString()}#${token.low.toString()}`;
}

/**
 * A comparison function used for determining sort order based on the current
 * locale's collation order.
 * @param {string} first
 * @param {string} second
 * @return {number} The result of the comparison.
 */
export function alphabeticalCompare(first, second) {
  return first.toLocaleLowerCase().localeCompare(second.toLocaleLowerCase());
}
