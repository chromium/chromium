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
   *
   * This function is public only to output classes.
   * @param {!Array<Spannable>} buff
   * @param {string|!Spannable} value
   * @param {{annotation: Array<*>, isUnique: (boolean|undefined)}=} opt_options
   */
  append(buff, value, opt_options) {}

  /**
   * This function is public only to output classes.
   * @param {string} text
   * @param {!AutomationNode} contextNode
   * @param {!Array<Spannable>} buff
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  assignLocaleAndAppend(text, contextNode, buff, options) {}

  /**
   * Find the earcon for a given node (including ancestry).
   *
   * This function is public only to output classes.
   * @param {!AutomationNode} node
   * @param {!AutomationNode=} opt_prevNode
   * @return {outputTypes.OutputAction}
   */
  findEarcon(node, opt_prevNode) {}

  /**
   * @param {!AutomationNode} node
   * @param {!AutomationNode} prevNode
   * @param {!outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff
   * @param {!OutputFormatLogger} formatLog
   */
  formatNode(node, prevNode, type, buff, formatLog) {}

  /**
   * Renders the given range using optional context previous range and event
   * type.
   *
   * This function is public only to output classes.
   * @param {!CursorRange} range
   * @param {CursorRange} prevRange
   * @param {EventType|outputTypes.OutputEventType} type
   * @param {!Array<Spannable>} buff Buffer to receive rendered output.
   * @param {!OutputFormatLogger} formatLog
   * @param {{suppressStartEndAncestry: (boolean|undefined)}} optionalArgs
   */
  render(range, prevRange, type, buff, formatLog, optionalArgs) {}

  /**
   * @param {string} token
   * @return {boolean}
   */
  shouldSuppress(token) {}

  /** @return {boolean} */
  get useAuralStyle() {}

  /** @return {boolean} */
  get formatAsBraille() {}

  /** @return {boolean} */
  get formatAsSpeech() {}
}
