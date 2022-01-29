// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {CloudOrigins, Destination, DestinationOrigin, GooglePromotedDestinationId, RecentDestination} from './destination.js';

/**
 * Printer types for capabilities and printer list requests.
 * Must match PrinterType in printing/print_job_constants.h
 * Note: PRIVET_PRINTER is deprecated.
 */
export enum PrinterType {
  PRIVET_PRINTER = 0,
  EXTENSION_PRINTER = 1,
  PDF_PRINTER = 2,
  LOCAL_PRINTER = 3,
  CLOUD_PRINTER = 4
}

export function originToType(origin: DestinationOrigin): PrinterType {
  if (origin === DestinationOrigin.LOCAL || origin === DestinationOrigin.CROS) {
    return PrinterType.LOCAL_PRINTER;
  }
  if (origin === DestinationOrigin.EXTENSION) {
    return PrinterType.EXTENSION_PRINTER;
  }
  assert(CloudOrigins.includes(origin));
  return PrinterType.CLOUD_PRINTER;
}

export function getPrinterTypeForDestination(
    destination: (Destination|RecentDestination)): PrinterType {
  // <if expr="chromeos_ash or chromeos_lacros">
  if (destination.id === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
    return PrinterType.PDF_PRINTER;
  }
  // </if>

  if (destination.id === GooglePromotedDestinationId.SAVE_AS_PDF) {
    return PrinterType.PDF_PRINTER;
  }
  return originToType(destination.origin);
}

export class DestinationMatch {
  private origins_: DestinationOrigin[];

  private idRegExp_: RegExp|null;

  private displayNameRegExp_: RegExp|null;

  private skipVirtualDestinations_: boolean;

  /**
   * A set of key parameters describing a destination used to determine
   * if two destinations are the same.
   * @param origins Match destinations from these origins.
   * @param idRegExp Match destination's id.
   * @param displayNameRegExp Match destination's displayName.
   * @param skipVirtualDestinations Whether to ignore virtual
   *     destinations, for example, Save as PDF.
   */
  constructor(
      origins: DestinationOrigin[], idRegExp: RegExp|null,
      displayNameRegExp: RegExp|null, skipVirtualDestinations: boolean) {
    this.origins_ = origins;
    this.idRegExp_ = idRegExp;
    this.displayNameRegExp_ = displayNameRegExp;
    this.skipVirtualDestinations_ = skipVirtualDestinations;
  }

  matchOrigin(origin: DestinationOrigin): boolean {
    return this.origins_.includes(origin);
  }

  matchIdAndOrigin(id: string, origin: DestinationOrigin): boolean {
    return this.matchOrigin(origin) && !!this.idRegExp_ &&
        this.idRegExp_.test(id);
  }

  match(destination: Destination): boolean {
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
   * @return Whether {@code destination} is virtual, in terms of
   *     destination selection.
   */
  private isVirtualDestination_(destination: Destination): boolean {
    // <if expr="chromeos_ash or chromeos_lacros">
    if (destination.id === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
      return true;
    }
    // </if>

    return destination.id === GooglePromotedDestinationId.DOCS ||
        destination.id === GooglePromotedDestinationId.SAVE_AS_PDF;
  }

  /**
   * @return The printer types that correspond to this destination match.
   */
  getTypes(): Set<PrinterType> {
    return new Set(this.origins_.map(originToType));
  }
}
