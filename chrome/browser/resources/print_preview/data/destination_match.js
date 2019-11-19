// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {CloudOrigins, Destination, DestinationOrigin} from './destination.js';

/**
 * Printer types for capabilities and printer list requests.
 * Must match PrinterType in printing/print_job_constants.h
 * @enum {number}
 */
export const PrinterType = {
  PRIVET_PRINTER: 0,
  EXTENSION_PRINTER: 1,
  PDF_PRINTER: 2,
  LOCAL_PRINTER: 3,
  CLOUD_PRINTER: 4
};

/**
 * Converts DestinationOrigin to PrinterType.
 * @param {!DestinationOrigin} origin The printer's destination origin.
 * return {!PrinterType} The corresponding PrinterType.
 */
export const originToType = function(origin) {
  if (origin === DestinationOrigin.LOCAL || origin === DestinationOrigin.CROS) {
    return PrinterType.LOCAL_PRINTER;
  }
  if (origin === DestinationOrigin.PRIVET) {
    return PrinterType.PRIVET_PRINTER;
  }
  if (origin === DestinationOrigin.EXTENSION) {
    return PrinterType.EXTENSION_PRINTER;
  }
  assert(CloudOrigins.includes(origin));
  return PrinterType.CLOUD_PRINTER;
};

/**
 * @param {!Destination} destination The destination to figure
 *     out the printer type of.
 * @return {!PrinterType} Map the destination to a PrinterType.
 */
export function getPrinterTypeForDestination(destination) {
  if (destination.id == Destination.GooglePromotedId.SAVE_AS_PDF) {
    return PrinterType.PDF_PRINTER;
  }
  return originToType(destination.origin);
}

export class DestinationMatch {
  /**
   * A set of key parameters describing a destination used to determine
   * if two destinations are the same.
   * @param {!Array<!DestinationOrigin>} origins Match
   *     destinations from these origins.
   * @param {RegExp} idRegExp Match destination's id.
   * @param {RegExp} displayNameRegExp Match destination's displayName.
   * @param {boolean} skipVirtualDestinations Whether to ignore virtual
   *     destinations, for example, Save as PDF.
   */
  constructor(origins, idRegExp, displayNameRegExp, skipVirtualDestinations) {
    /** @private {!Array<!DestinationOrigin>} */
    this.origins_ = origins;

    /** @private {RegExp} */
    this.idRegExp_ = idRegExp;

    /** @private {RegExp} */
    this.displayNameRegExp_ = displayNameRegExp;

    /** @private {boolean} */
    this.skipVirtualDestinations_ = skipVirtualDestinations;
  }

  /**
   * @param {!DestinationOrigin} origin Origin to match.
   * @return {boolean} Whether the origin is one of the {@code origins_}.
   */
  matchOrigin(origin) {
    return this.origins_.includes(origin);
  }

  /**
   * @param {string} id Id of the destination.
   * @param {!DestinationOrigin} origin Origin of the
   *     destination.
   * @return {boolean} Whether destination is the same as initial.
   */
  matchIdAndOrigin(id, origin) {
    return this.matchOrigin(origin) && !!this.idRegExp_ &&
        this.idRegExp_.test(id);
  }

  /**
   * @param {!Destination} destination Destination to match.
   * @return {boolean} Whether {@code destination} matches the last user
   *     selected one.
   */
  match(destination) {
    if (!this.matchOrigin(destination.origin)) {
      return false;
    }
    if (this.idRegExp_ && !this.idRegExp_.test(destination.id)) {
      return false;
    }
    if (this.displayNameRegExp_ &&
        !this.displayNameRegExp_.test(destination.displayName)) {
      return false;
    }
    if (this.skipVirtualDestinations_ &&
        this.isVirtualDestination_(destination)) {
      return false;
    }
    return true;
  }

  /**
   * @param {!Destination} destination Destination to check.
   * @return {boolean} Whether {@code destination} is virtual, in terms of
   *     destination selection.
   * @private
   */
  isVirtualDestination_(destination) {
    if (destination.origin === DestinationOrigin.LOCAL) {
      return destination.id === Destination.GooglePromotedId.SAVE_AS_PDF;
    }
    return destination.id === Destination.GooglePromotedId.DOCS;
  }

  /**
   * @return {!Set<!PrinterType>} The printer types that
   *     correspond to this destination match.
   */
  getTypes() {
    return new Set(this.origins_.map(originToType));
  }
}
