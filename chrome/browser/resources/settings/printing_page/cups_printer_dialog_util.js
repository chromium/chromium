// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview  Utility functions that are used in Cups printer setup dialogs.
 */

cr.define('settings.printing', function() {
  /**
   * @param {string} protocol
   * @return {boolean} Whether |protocol| is a network protocol
   */
  function isNetworkProtocol(protocol) {
    return ['ipp', 'ipps', 'http', 'https', 'socket', 'lpd'].includes(protocol);
  }

  /**
   * Returns true if the printer's name and address is valid. This function
   * uses regular expressions to determine whether the provided printer name
   * and address are valid. Address can be either an ipv4/6 address or a
   * hostname followed by an optional port.
   * NOTE: The regular expression for hostnames will allow hostnames that are
   * over 255 characters.
   * @param {CupsPrinterInfo} printer
   * @return {boolean}
   */
  function isNameAndAddressValid(printer) {
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

    // Matches an arbitrary number of 'prefix patterns' which are separated by a
    // dot.
    const hostnameSuffix = `(\\.${hostnamePrefix})*`;

    // Matches an optional port at the end of the address.
    const portNumber = '(:\\d+)?';

    const ipv6Full = '(([a-f\\d]){1,4}(:(:)?([a-f\\d]){1,4}){1,7})';

    // Special cases for addresses using a shorthand notation.
    const ipv6Prefix = '(::([a-f\\d]){1,4})';
    const ipv6Suffix = '(([a-f\\d]){1,4}::)';
    const ipv6Combined = `(${ipv6Full}|${ipv6Prefix}|${ipv6Suffix})`;
    const ipv6WithPort = `(\\[${ipv6Combined}\\]${portNumber})`;

    // Matches valid hostnames and ipv4 addresses.
    const hostnameRegex =
        new RegExp(`^${hostnamePrefix}${hostnameSuffix}${portNumber}$`, 'i');

    // Matches valid ipv6 addresses.
    const ipv6AddressRegex =
        new RegExp(`^(${ipv6Combined}|${ipv6WithPort})$`, 'i');

    const invalidIpv6Regex = new RegExp('.*::.*::.*');

    return hostnameRegex.test(address) ||
        (ipv6AddressRegex.test(address) && !invalidIpv6Regex.test(address));
  }

  /**
   * Returns true if the printer's manufacturer and model or ppd path is valid.
   * @param {string} manufacturer
   * @param {string} model
   * @param {string} ppdPath
   * @return {boolean}
   */
  function isPPDInfoValid(manufacturer, model, ppdPath) {
    return !!((manufacturer && model) || ppdPath);
  }

  /**
   * Returns the base name of a filepath.
   * @param {string} path The full path of the file
   * @return {string} The base name of the file
   */
  function getBaseName(path) {
    if (path && path.length > 0) {
      return path.substring(path.lastIndexOf('/') + 1);
    }
    return '';
  }

  /**
   * A function used for sorting printer names based on the current locale's
   * collation order.
   * @param {!CupsPrinterInfo} first
   * @param {!CupsPrinterInfo} second
   * @return {number} The result of the comparison.
   */
  function alphabeticalSort(first, second) {
    return first.printerName.toLocaleLowerCase().localeCompare(
        second.printerName.toLocaleLowerCase());
  }

  /**
   * Return the error string corresponding to the result code.
   * @param {!PrinterSetupResult} result
   * @return {string}
   */
  function getErrorText(result) {
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
        assertNotReached();
    }
  }

  /**
   * We sort by printer type, which is based off of a maintained list in
   * cups_printers_types.js. If the types are the same, we sort alphabetically.
   * @param {!PrinterListEntry} first
   * @param {!PrinterListEntry} second
   * @return {number}
   */
  function sortPrinters(first, second) {
    if (first.printerType == second.printerType) {
      return settings.printing.alphabeticalSort(
          first.printerInfo, second.printerInfo);
    }

    return first.printerType - second.printerType;
  }

  /**
   * @param {!CupsPrinterInfo} printer
   * @param {string} searchTerm
   * @return {boolean} True if the printer has |searchTerm| in its name.
   */
  function matchesSearchTerm(printer, searchTerm) {
    return printer.printerName.toLowerCase().includes(searchTerm.toLowerCase());
  }

  /**
   * @param {!PrinterListEntry} first
   * @param {!PrinterListEntry} second
   * @return {boolean}
   */
  function arePrinterIdsEqual(first, second) {
    return first.printerInfo.printerId == second.printerInfo.printerId;
  }

  return {
    isNetworkProtocol: isNetworkProtocol,
    isNameAndAddressValid: isNameAndAddressValid,
    isPPDInfoValid: isPPDInfoValid,
    getBaseName: getBaseName,
    alphabeticalSort: alphabeticalSort,
    getErrorText: getErrorText,
    sortPrinters: sortPrinters,
    matchesSearchTerm: matchesSearchTerm,
    arePrinterIdsEqual: arePrinterIdsEqual,
  };
});
