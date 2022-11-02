// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface for the central output class for ChromeVox.
 */
import {OutputFormatTree} from './output_format_tree.js';
import {OutputFormattingData} from './output_types.js';

/** @interface */
export class OutputInterface {
  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatAsFieldAccessor_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatAsStateValue_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatCellIndexText_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   */
  formatChecked_(data, token) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatCustomFunction_(data, token, tree, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   */
  formatDescendants_(data, token) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatDescription_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   */
  formatFind_(data, token, tree) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatIndexInParent_(data, token, tree, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatInputType_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatJoinedDescendants_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   */
  formatListNestedLevel_(data) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatMessage_(data, token, tree, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatName_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatNameFromNode_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatNameOrDescendants_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatNode_(data, token, tree, options) {}

  /**
   * @param {!OutputFormattingData} data
   */
  formatPhoneticReading_(data) {}

  /**
   * @param {!OutputFormattingData} data
   */
  formatPrecedingBullet_(data) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   */
  formatPressed_(data, token) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   */
  formatRestriction_(data, token) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatRole_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   */
  formatState_(data, token) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatTableCellIndex_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatTextContent_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatUrlFilename_(data, token, options) {}

  /**
   * @param {!OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatValue_(data, token, options) {}

  /**
   * @param {string} token
   * @return {boolean}
   */
  shouldSuppress(token) {}

  /** @return {boolean} */
  get useAuralStyle() {}
}
