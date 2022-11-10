// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface for the central output class for ChromeVox.
 */
import {Spannable} from '../../common/spannable.js';

import {OutputFormatTree} from './output_format_tree.js';
import {OutputAction, OutputFormattingData} from './output_types.js';

const AutomationNode = chrome.automation.AutomationNode;

/** @interface */
export class OutputInterface {
  /**
   * Appends output to the |buff|.
   * @param {!Array<Spannable>} buff
   * @param {string|!Spannable} value
   * @param {{annotation: Array<*>, isUnique: (boolean|undefined)}=} opt_options
   */
  append_(buff, value, opt_options) {}

  /**
   * @param {string} text
   * @param {!AutomationNode} contextNode
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  assignLocaleAndAppend_(text, contextNode, buff, options) {}

  /**
   * Find the earcon for a given node (including ancestry).
   * @param {!AutomationNode} node
   * @param {!AutomationNode=} opt_prevNode
   * @return {OutputAction}
   */
  findEarcon_(node, opt_prevNode) {}

  /**
   * Format the node given the format specifier.
   * @param {!OutputFormattingData} params All the required and
   *     optional parameters for formatting.
   */
  format_(params) {}

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
   * @param {string} token
   * @return {boolean}
   */
  shouldSuppress(token) {}

  /** @return {boolean} */
  get useAuralStyle() {}

  /** @return {boolean} */
  get formatAsBraille() {}
}
