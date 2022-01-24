// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {isChromeOS, isLacros} from 'chrome://resources/js/cr.m.js';

import {Destination, DestinationConnectionStatus, DestinationOptionalParams, DestinationOrigin, DestinationProvisionalType, DestinationType} from './destination.js';
import {PrinterType} from './destination_match.js';

type ObjectMap = {
  [k: string]: any
};

export type LocalDestinationInfo = {
  deviceName: string,
  printerName: string,
  printerDescription?: string,
  cupsEnterprisePrinter?: boolean,
  printerOptions?: ObjectMap,
};

export type ProvisionalDestinationInfo = {
  extensionId: string,
  extensionName: string,
  id: string,
  name: string,
  description?: string,
  provisional?: boolean,
};

/**
 * @param type The type of printer to parse.
 * @param printer Information about the printer.
 *       Type expected depends on |type|:
 *       For LOCAL_PRINTER => LocalDestinationInfo
 *       For EXTENSION_PRINTER => ProvisionalDestinationInfo
 * @return Destination, or null if an invalid value is provided for |type|.
 */
export function parseDestination(
    type: PrinterType,
    printer: (LocalDestinationInfo|ProvisionalDestinationInfo)): (Destination|
                                                                  null) {
  if (type === PrinterType.LOCAL_PRINTER) {
    return parseLocalDestination(printer as LocalDestinationInfo);
  }
  if (type === PrinterType.EXTENSION_PRINTER) {
    return parseExtensionDestination(printer as ProvisionalDestinationInfo);
  }
  assertNotReached('Unknown printer type ' + type);
  return null;
}

/**
 * @param destinationInfo Information describing a local print destination.
 * @return Parsed local print destination.
 */
function parseLocalDestination(destinationInfo: LocalDestinationInfo):
    Destination {
  const options: DestinationOptionalParams = {
    description: destinationInfo.printerDescription,
    isEnterprisePrinter: destinationInfo.cupsEnterprisePrinter,
  };
  if (destinationInfo.printerOptions) {
    // Convert options into cloud print tags format.
    options.tags =
        Object.keys(destinationInfo.printerOptions).map(function(key) {
          return '__cp__' + key + '=' + destinationInfo.printerOptions![key];
        });
  }
  return new Destination(
      destinationInfo.deviceName, DestinationType.LOCAL,
      (isChromeOS || isLacros) ? DestinationOrigin.CROS :
                                 DestinationOrigin.LOCAL,
      destinationInfo.printerName, DestinationConnectionStatus.ONLINE, options);
}

/**
 * Parses an extension destination from an extension supplied printer
 * description.
 */
export function parseExtensionDestination(
    destinationInfo: ProvisionalDestinationInfo): Destination {
  const provisionalType = destinationInfo.provisional ?
      DestinationProvisionalType.NEEDS_USB_PERMISSION :
      DestinationProvisionalType.NONE;

  return new Destination(
      destinationInfo.id, DestinationType.LOCAL, DestinationOrigin.EXTENSION,
      destinationInfo.name, DestinationConnectionStatus.ONLINE, {
        description: destinationInfo.description || '',
        extensionId: destinationInfo.extensionId,
        extensionName: destinationInfo.extensionName || '',
        provisionalType: provisionalType
      });
}
