// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Destination} from './destination.js';
import {PrinterType} from './destination.js';

export class DestinationMatch {
  private idRegExp_: RegExp|null;

  private displayNameRegExp_: RegExp|null;

  /**
   * A set of key parameters describing a destination used to determine
   * if two destinations are the same.
   * @param idRegExp Match destination's id.
   * @param displayNameRegExp Match destination's displayName.
   */
  constructor(idRegExp: RegExp|null, displayNameRegExp: RegExp|null) {
    this.idRegExp_ = idRegExp;
    this.displayNameRegExp_ = displayNameRegExp;
  }

  match(destination: Destination): boolean {
    if (this.idRegExp_ && !this.idRegExp_.test(destination.id)) {
      return false;
    }
    if (this.displayNameRegExp_ &&
        !this.displayNameRegExp_.test(destination.displayName)) {
      return false;
    }
    if (destination.type === PrinterType.PDF_PRINTER) {
      return false;
    }
    return true;
  }
}
