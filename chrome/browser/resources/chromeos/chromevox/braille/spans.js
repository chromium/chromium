// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Objects used in spannables as annotations for ARIA values
 * and selections.
 */

goog.provide('BrailleTextStyleSpan');
goog.provide('ExtraCellsSpan');
goog.provide('ValueSelectionSpan');
goog.provide('ValueSpan');

goog.require('LibLouis.FormType');
goog.require('Spannable');

/**
 * Attached to the value region of a braille spannable.
 * @param {number} offset The offset of the span into the value.
 * @constructor
 */
ValueSpan = function(offset) {
  /**
   * The offset of the span into the value.
   * @type {number}
   */
  this.offset = offset;
};


/**
 * Creates a value span from a json serializable object.
 * @param {!Object} obj The json serializable object to convert.
 * @return {!ValueSpan} The value span.
 */
ValueSpan.fromJson = function(obj) {
  return new ValueSpan(obj.offset);
};


/**
 * Converts this object to a json serializable object.
 * @return {!Object} The JSON representation.
 */
ValueSpan.prototype.toJson = function() {
  return this;
};


Spannable.registerSerializableSpan(
    ValueSpan, 'ValueSpan', ValueSpan.fromJson, ValueSpan.prototype.toJson);


/**
 * Attached to the selected text within a value.
 * @constructor
 */
ValueSelectionSpan = function() {};


Spannable.registerStatelessSerializableSpan(
    ValueSelectionSpan, 'ValueSelectionSpan');


/**
 * Causes raw cells to be added when translating from text to braille.
 * This is supported by the {@code ExpandingBrailleTranslator}
 * class.
 * @constructor
 */
ExtraCellsSpan = function() {
  /** @type {ArrayBuffer} */
  this.cells = new Uint8Array(0).buffer;
};


/**
 * Indicates a text form during translation in Liblouis.
 * @param {LibLouis.FormType} formType
 * @constructor
 */
BrailleTextStyleSpan = function(formType) {
  /** @type {LibLouis.FormType} */
  this.formType = formType;
};
