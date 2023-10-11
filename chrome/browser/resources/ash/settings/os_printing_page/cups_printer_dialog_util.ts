// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {PrinterListEntry} from './cups_printer_types.js';
import {CupsPrinterInfo, PrinterSetupResult, PrintServerResult} from './cups_printers_browser_proxy.js';

/**
 * @fileoverview  Utility functions that are used in Cups printer setup dialogs.
 */

export function isNetworkProtocol(protocol: string): boolean {
  return ['ipp', 'ipps', 'http', 'https', 'socket', 'lpd'].includes(protocol);
}

/**
 * @return Returns true if the printer's name and address is valid. This
 * function uses regular expressions to determine whether the provided printer
 * name and address are valid. Address can be either an ipv4/6 address or a
 * hostname followed by an optional port.
 * NOTE: The regular expression for hostnames will allow hostnames that are
 * over 255 characters.
 */
export function isNameAndAddressValid(printer: CupsPrinterInfo): boolean {
  if (!printer) {
    return false;
  }

  const name = printer.printerName;
  const address = printer.printerAddress;

  if (!isNetworkProtocol(printer.printerProtocol) && !!name) {
    // We do not need to verify the address of a non-network printer.
    return true;
  }

  if (!name || !address) {
    return false;
  }

  const hostnamePrefix = '([a-z\\d]|[a-z\\d][a-z\\d\\-]{0,61}[a-z\\d])';

  /**
   * Matches an arbitrary number of 'prefix patterns' which are separated by a
   * dot.
   */
  const hostnameSuffix = `(\\.${hostnamePrefix})*`;

  /**
   * Matches an optional port at the end of the address.
   */
  const portNumber = '(:\\d+)?';

  const ipv6Full = '(([a-f\\d]){1,4}(:(:)?([a-f\\d]){1,4}){1,7})';

  /**
   * Special cases for addresses using a shorthand notation.
   */
  const ipv6Prefix = '(::([a-f\\d]){1,4})';
  const ipv6Suffix = '(([a-f\\d]){1,4}::)';
  const ipv6Combined = `(${ipv6Full}|${ipv6Prefix}|${ipv6Suffix})`;
  const ipv6WithPort = `(\\[${ipv6Combined}\\]${portNumber})`;

  /**
   * Matches valid hostnames and ipv4 addresses.
   */
  const hostnameRegex =
      new RegExp(`^${hostnamePrefix}${hostnameSuffix}${portNumber}$`, 'i');

  /**
   * Matches valid ipv6 addresses.
   */
  const ipv6AddressRegex =
      new RegExp(`^(${ipv6Combined}|${ipv6WithPort})$`, 'i');

  const invalidIpv6Regex = new RegExp('.*::.*::.*');

  return hostnameRegex.test(address) ||
      (ipv6AddressRegex.test(address) && !invalidIpv6Regex.test(address));
}

/**
 * @return Returns true if the printer's manufacturer and model or ppd path is
 *     valid.
 */
export function isPPDInfoValid(
    manufacturer: string, model: string, ppdPath: string): boolean {
  return !!((manufacturer && model) || ppdPath);
}

/**
 * @return Returns the base name of a filepath.
 */
export function getBaseName(path: string): string {
  if (path && path.length > 0) {
    return path.substring(path.lastIndexOf('/') + 1);
  }
  return '';
}

/**
 * A function used for sorting printer names based on the current locale's
 * collation order.
 */
function alphabeticalSort(
    first: CupsPrinterInfo, second: CupsPrinterInfo): number {
  return first.printerName.toLocaleLowerCase().localeCompare(
      second.printerName.toLocaleLowerCase());
}

/**
 * @return Return the error string corresponding to the result code.
 */
export function getErrorText(result: PrinterSetupResult): string {
  switch (result) {
    case PrinterSetupResult.FATAL_ERROR:
      return loadTimeData.getString('printerAddedFatalErrorMessage');
    case PrinterSetupResult.PRINTER_UNREACHABLE:
      return loadTimeData.getString('printerAddedUnreachableMessage');
    case PrinterSetupResult.DBUS_ERROR:
      // Simply return a generic error message as this error should only
      // occur when a call to Dbus fails which isn't meaningful to the user.
      return loadTimeData.getString('printerAddedFailedMessage');
    case PrinterSetupResult.NATIVE_PRINTERS_NOT_ALLOWED:
      return loadTimeData.getString(
          'printerAddedNativePrintersNotAllowedMessage');
    case PrinterSetupResult.INVALID_PRINTER_UPDATE:
      return loadTimeData.getString('editPrinterInvalidPrinterUpdate');
    case PrinterSetupResult.PPD_TOO_LARGE:
      return loadTimeData.getString('printerAddedPpdTooLargeMessage');
    case PrinterSetupResult.INVALID_PPD:
      return loadTimeData.getString('printerAddedInvalidPpdMessage');
    case PrinterSetupResult.PPD_NOT_FOUND:
      return loadTimeData.getString('printerAddedPpdNotFoundMessage');
    case PrinterSetupResult.PPD_UNRETRIEVABLE:
      return loadTimeData.getString('printerAddedPpdUnretrievableMessage');
    default:
      // TODO(b/277073603): As part of the OS Printer settings revamp, add
      // strings for the missing `PrinterSetupResult` values.
      return loadTimeData.getString('printerAddedFailedMessage');
  }
}

/**
 * @return Return the error string corresponding to the result code for print
 *     servers.
 */
export function getPrintServerErrorText(result: PrintServerResult): string {
  switch (result) {
    case PrintServerResult.CONNECTION_ERROR:
      return loadTimeData.getString('printServerConnectionError');
    case PrintServerResult.CANNOT_PARSE_IPP_RESPONSE:
    case PrintServerResult.HTTP_ERROR:
      return loadTimeData.getString('printServerConfigurationErrorMessage');
    default:
      assertNotReached();
  }
}

/**
 * We sort by printer type, which is based off of a maintained list in
 * cups_printers_types.js. If the types are the same, we sort alphabetically.
 */
export function sortPrinters(
    first: PrinterListEntry, second: PrinterListEntry): number {
  if (first.printerType === second.printerType) {
    return alphabeticalSort(first.printerInfo, second.printerInfo);
  }

  return first.printerType - second.printerType;
}

export function matchesSearchTerm(
    printer: CupsPrinterInfo, searchTerm: string): boolean {
  return printer.printerName.toLowerCase().includes(searchTerm.toLowerCase());
}

export function arePrinterIdsEqual(
    first: PrinterListEntry, second: PrinterListEntry): boolean {
  return first.printerInfo.printerId === second.printerInfo.printerId;
}

/**
 * Finds the printers that are in |firstArr| but not in |secondArr|.
 */
export function findDifference(
    firstArr: PrinterListEntry[],
    secondArr: PrinterListEntry[]): PrinterListEntry[] {
  return firstArr.filter(p1 => {
    return !secondArr.some(
        p2 => p2.printerInfo.printerId === p1.printerInfo.printerId);
  });
}
