// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

/**
 * Converts a chromeos.scanning.mojom.ColorMode string to the corresponding enum
 * value.
 * @param {string} colorModeString
 * @return {chromeos.scanning.mojom.ColorMode}
 */
export function colorModeFromString(colorModeString) {
  switch (colorModeString) {
    case chromeos.scanning.mojom.ColorMode.kBlackAndWhite.toString():
      return chromeos.scanning.mojom.ColorMode.kBlackAndWhite;
    case chromeos.scanning.mojom.ColorMode.kGrayscale.toString():
      return chromeos.scanning.mojom.ColorMode.kGrayscale;
    case chromeos.scanning.mojom.ColorMode.kColor.toString():
      return chromeos.scanning.mojom.ColorMode.kColor;
    default:
      assertNotReached();
      return chromeos.scanning.mojom.ColorMode.kColor;
  }
}

/**
 * Converts a chromeos.scanning.mojom.FileType string to the corresponding
 * enum value.
 * @param {string} fileTypeString
 * @return {chromeos.scanning.mojom.FileType}
 */
export function fileTypeFromString(fileTypeString) {
  switch (fileTypeString) {
    case chromeos.scanning.mojom.FileType.kJpg.toString():
      return chromeos.scanning.mojom.FileType.kJpg;
    case chromeos.scanning.mojom.FileType.kPdf.toString():
      return chromeos.scanning.mojom.FileType.kPdf;
    case chromeos.scanning.mojom.FileType.kPng.toString():
      return chromeos.scanning.mojom.FileType.kPng;
    default:
      assertNotReached();
      return chromeos.scanning.mojom.FileType.kPdf;
  }
}

/**
 * Converts a chromeos.scanning.mojom.ColorMode to a string that can be
 * displayed in the color mode dropdown.
 * @param {chromeos.scanning.mojom.ColorMode} mojoColorMode
 * @return {string}
 */
export function getColorModeString(mojoColorMode) {
  switch (mojoColorMode) {
    case chromeos.scanning.mojom.ColorMode.kBlackAndWhite:
      return loadTimeData.getString('blackAndWhiteOptionText');
    case chromeos.scanning.mojom.ColorMode.kGrayscale:
      return loadTimeData.getString('grayscaleOptionText');
    case chromeos.scanning.mojom.ColorMode.kColor:
      return loadTimeData.getString('colorOptionText');
    default:
      assertNotReached();
      return loadTimeData.getString('blackAndWhiteOptionText');
  }
}

/**
 * Converts a chromeos.scanning.mojom.PageSize to a string that can be
 * displayed in the page size dropdown.
 * @param {chromeos.scanning.mojom.PageSize} pageSize
 * @return {string}
 */
export function getPageSizeString(pageSize) {
  switch (pageSize) {
    case chromeos.scanning.mojom.PageSize.kIsoA4:
      return loadTimeData.getString('a4OptionText');
    case chromeos.scanning.mojom.PageSize.kNaLetter:
      return loadTimeData.getString('letterOptionText');
    case chromeos.scanning.mojom.PageSize.kMax:
      return loadTimeData.getString('fitToScanAreaOptionText');
    default:
      assertNotReached();
      return loadTimeData.getString('letterOptionText');
  }
}

/**
 * Converts a chromeos.scanning.mojom.SourceType to a string that can be
 * displayed in the source dropdown.
 * @param {chromeos.scanning.mojom.SourceType} mojoSourceType
 * @return {string}
 */
export function getSourceTypeString(mojoSourceType) {
  switch (mojoSourceType) {
    case chromeos.scanning.mojom.SourceType.kFlatbed:
      return loadTimeData.getString('flatbedOptionText');
    case chromeos.scanning.mojom.SourceType.kAdfSimplex:
      return loadTimeData.getString('oneSidedDocFeederOptionText');
    case chromeos.scanning.mojom.SourceType.kAdfDuplex:
      return loadTimeData.getString('twoSidedDocFeederOptionText');
    case chromeos.scanning.mojom.SourceType.kDefault:
      return loadTimeData.getString('defaultSourceOptionText');
    case chromeos.scanning.mojom.SourceType.kUnknown:
    default:
      assertNotReached();
      return loadTimeData.getString('defaultSourceOptionText');
  }
}

/**
 * Converts a chromeos.scanning.mojom.PageSize string to the corresponding enum
 * value.
 * @param {string} pageSizeString
 * @return {chromeos.scanning.mojom.PageSize}
 */
export function pageSizeFromString(pageSizeString) {
  switch (pageSizeString) {
    case chromeos.scanning.mojom.PageSize.kIsoA4.toString():
      return chromeos.scanning.mojom.PageSize.kIsoA4;
    case chromeos.scanning.mojom.PageSize.kNaLetter.toString():
      return chromeos.scanning.mojom.PageSize.kNaLetter;
    case chromeos.scanning.mojom.PageSize.kMax.toString():
      return chromeos.scanning.mojom.PageSize.kMax;
    default:
      assertNotReached();
      return chromeos.scanning.mojom.PageSize.kNaLetter;
  }
}

/**
 * Converts a scanner's display name from UTF-16 to a displayable string.
 * @param {!chromeos.scanning.mojom.Scanner} scanner
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
