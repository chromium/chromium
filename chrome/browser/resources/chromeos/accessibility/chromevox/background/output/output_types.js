// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions of all types related to output.
 */

import {EarconId} from '../../common/earcon_id.js';
import {Spannable} from '../../common/spannable.js';
import {ChromeVox} from '../chromevox.js';

import {OutputFormatTree} from './output_format_tree.js';
import {OutputFormatLogger} from './output_logger.js';

const AriaCurrentState = chrome.automation.AriaCurrentState;
const AutomationNode = chrome.automation.AutomationNode;
const Restriction = chrome.automation.Restriction;

/**
 * The ordering of contextual output.
 * @enum {string}
 */
export const OutputContextOrder = {
  // The (ancestor) context comes before the node output.
  FIRST: 'first',
  // The (ancestor) context comes before the node output when moving forward,
  // after when moving backward.
  DIRECTED: 'directed',

  // The (ancestor) context comes after the node output.
  LAST: 'last',

  // Ancestor context is placed both before and after node output.
  FIRST_AND_LAST: 'firstAndLast',
};

/**
 * Used to annotate utterances with speech properties.
 */
export class OutputSpeechProperties {
  constructor() {
    /** @private {!Object} */
    this.properties_ = {};
  }

  /** @return {!Object} */
  get properties() {
    return this.properties_;
  }

  /** @override */
  toJSON() {
    // Make a copy of our properties since the caller really shouldn't be
    // modifying our local state.
    const clone = {};
    for (const key in this.properties_) {
      clone[key] = this.properties_[key];
    }
    return clone;
  }
}

/**
 * Custom actions performed while rendering an output string.
 */
export class OutputAction {
  run() {}
}

/**
 * Action to play an earcon.
 */
export class OutputEarconAction extends OutputAction {
  /**
   * @param {!EarconId} earcon
   * @param {chrome.automation.Rect=} opt_location
   */
  constructor(earcon, opt_location) {
    super();

    /** @type {!EarconId} */
    this.earcon = earcon;
    /** @type {chrome.automation.Rect|undefined} */
    this.location = opt_location;
  }

  /** @override */
  run() {
    ChromeVox.earcons.playEarcon(this.earcon, this.location);
  }

  /** @override */
  toJSON() {
    return {earcon: this.earcon};
  }
}

/**
 * Annotation for text with a selection inside it.
 */
export class OutputSelectionSpan {
  /**
   * @param {number} startIndex
   * @param {number} endIndex
   */
  constructor(startIndex, endIndex) {
    // TODO(dtseng): Direction lost below; should preserve for braille panning.
    this.startIndex = startIndex < endIndex ? startIndex : endIndex;
    this.endIndex = endIndex > startIndex ? endIndex : startIndex;
  }
}

/**
 * Wrapper for automation nodes as annotations.  Since the
 * {@code chrome.automation.AutomationNode} constructor isn't exposed in the
 * API, this class is used to allow instanceof checks on these annotations.
 */
export class OutputNodeSpan {
  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {number=} opt_offset Offsets into the node's text. Defaults to 0.
   */
  constructor(node, opt_offset) {
    this.node = node;
    this.offset = opt_offset ? opt_offset : 0;
  }
}

/**
 * Possible events handled by ChromeVox internally.
 * @enum {string}
 */
export const OutputCustomEvent = {
  NAVIGATE: 'navigate',
};

/** @typedef {!chrome.automation.EventType|!OutputCustomEvent} */
export let OutputEventType;

/**
 * Rules for mapping properties to a msg id
 * @const {Object<Object<string, string>>}
 */
export const OutputPropertyMap = {
  CHECKED: {
    'true': 'checked_true',
    'false': 'checked_false',
    'mixed': 'checked_mixed',
  },
  PRESSED: {
    'true': 'aria_pressed_true',
    'false': 'aria_pressed_false',
    'mixed': 'aria_pressed_mixed',
  },
  RESTRICTION: {
    [Restriction.DISABLED]: 'aria_disabled_true',
    [Restriction.READ_ONLY]: 'aria_readonly_true',
  },
  STATE: {
    [AriaCurrentState.TRUE]: 'aria_current_true',
    [AriaCurrentState.PAGE]: 'aria_current_page',
    [AriaCurrentState.STEP]: 'aria_current_step',
    [AriaCurrentState.LOCATION]: 'aria_current_location',
    [AriaCurrentState.DATE]: 'aria_current_date',
    [AriaCurrentState.TIME]: 'aria_current_time',
  },
};

/**
 * Metadata about supported automation states.
 * @const {!Object<string, {on: {msgId: string, earcon: !EarconId},
 *                          off: {msgId: string, earcon: !EarconId},
 *                          isRoleSpecific: (boolean|undefined)}>}
 *     on: info used to describe a state that is set to true.
 *     off: info used to describe a state that is set to undefined.
 *     isRoleSpecific: info used for specific roles.
 */
export const OUTPUT_STATE_INFO = {
  collapsed: {on: {msgId: 'aria_expanded_false'}},
  default: {on: {msgId: 'default_state'}},
  expanded: {on: {msgId: 'aria_expanded_true'}},
  multiselectable: {on: {msgId: 'aria_multiselectable_true'}},
  required: {on: {msgId: 'aria_required_true'}},
  visited: {on: {msgId: 'visited_state'}},
};

/**
 * Maps input types to message IDs.
 * @const {Object<string, string>}
 */
export const INPUT_TYPE_MESSAGE_IDS = {
  'email': 'input_type_email',
  'number': 'input_type_number',
  'password': 'input_type_password',
  'search': 'input_type_search',
  'tel': 'input_type_number',
  'text': 'input_type_text',
  'url': 'input_type_url',
};

/**
 * @typedef {{
 *    node: chrome.automation.AutomationNode,
 *    outputFormat: (string|!OutputFormatTree),
 *    outputBuffer: !Array<Spannable>,
 *    outputFormatLogger: !OutputFormatLogger,
 *    opt_prevNode: (!AutomationNode|undefined),
 *    opt_speechProps: (OutputSpeechProperties|undefined)
 * }}
 *
 * node: The AutomationNode of interest.
 * outputFormat: The output format either specified as an output template
 *     string or a parsed output format tree.
 * outputBuffer: Buffer to receive rendered output.
 * outputFormatLogger: Used for logging and recording output.
 * opt_prevNode: Optional argument. Helps provide context for certain speech
 *     output.
 * opt_speechProps: Optional argument. Used to specify how speech should be
 *     verbalized; can specify pitch, rate, language, etc.
 */
export let OutputFormattingData;

/** @enum {string} */
export const OutputFormatType = {
  BRAILLE: 'braille',
  SPEAK: 'speak',
};

/** @enum {string} */
export const OutputNavigationType = {
  END_OF: 'endOf',
  ENTER: 'enter',
  LEAVE: 'leave',
  START_OF: 'startOf',
};
