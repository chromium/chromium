// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface for the central output class for ChromeVox.
 */
import {CursorRange} from '../../../common/cursors/range.js';
import {Spannable} from '../../common/spannable.js';

import {OutputFormatTree} from './output_format_tree.js';
import {OutputFormatLogger} from './output_logger.js';
import * as outputTypes from './output_types.js';

const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;

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
   * @return {outputTypes.OutputAction}
   */
  findEarcon_(node, opt_prevNode) {}

  /**
   * Format the node given the format specifier.
   * @param {!outputTypes.OutputFormattingData} params All the required and
   *     optional parameters for formatting.
   */
  format_(params) {}

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatAsFieldAccessor_(data, token, options) {}

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatAsStateValue_(data, token, options) {}

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatCustomFunction_(data, token, tree, options) {}

  /**
   * @param {!outputTypes.OutputFormattingData} data
   */
  formatListNestedLevel_(data) {}

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatMessage_(data, token, tree, options) {}

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatNode_(data, token, tree, options) {}

  /**
   * @param {!outputTypes.OutputFormattingData} data
   */
  formatPhoneticReading_(data) {}

  /**
   * @param {!outputTypes.OutputFormattingData} data
   */
  formatPrecedingBullet_(data) {}

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatTextContent_(data, token, options) {}

  /**
   * Renders the given range using optional context previous range and event
   * type.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff Buffer to receive rendered output.
   * @param {!OutputFormatLogger} formatLog
   * @param {{suppressStartEndAncestry: (boolean|undefined)}} optionalArgs
   */
  render_(range, prevRange, type, buff, formatLog, optionalArgs) {}

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
