// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Objects used in spannables as annotations for ARIA values
 * and selections.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import type {SerializedAnnotation} from '../../common/spannable.js';
import {Spannable} from '../../common/spannable.js';

import type {LibLouis} from './liblouis.js';

/** Attached to the value region of a braille spannable. */
export class ValueSpan {
  /** The offset of the span into the value. */
  offset: number;

  constructor(offset: number) {
    this.offset = offset;
  }

  /**
   * Creates a value span from a json serializable object.
   * @param obj The json serializable object to convert.
   * @return The value span.
   */
  static fromJson(obj: SerializedAnnotation): ValueSpan {
    return new ValueSpan(obj.offset);
  }

  /**
   * Converts this object to a json serializable object.
   * @return The JSON representation.
   */
  toJson(): SerializedAnnotation {
    return this;
  }
}

Spannable.registerSerializableSpan(
    ValueSpan, 'ValueSpan', ValueSpan.fromJson, ValueSpan.prototype.toJson);


/** Attached to the selected text within a value. */
export class ValueSelectionSpan {}


Spannable.registerStatelessSerializableSpan(
    ValueSelectionSpan, 'ValueSelectionSpan');


/**
 * Causes raw cells to be added when translating from text to braille.
 * This is supported by the {@code ExpandingBrailleTranslator}
 * class.
 */
export class ExtraCellsSpan {
  cells: ArrayBuffer;

  constructor() {
    this.cells = new Uint8Array(0).buffer;
  }
}


/** Indicates a text form during translation in Liblouis. */
export class BrailleTextStyleSpan {
  formType: LibLouis.FormType;

  constructor(formType: LibLouis.FormType) {
    this.formType = formType;
  }
}

TestImportManager.exportForTesting(
    BrailleTextStyleSpan, ExtraCellsSpan, ValueSelectionSpan, ValueSpan);
