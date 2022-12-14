// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class that formats the parsed output tree.
 */
import {AutomationPredicate} from '../../../common/automation_predicate.js';
import {AutomationUtil} from '../../../common/automation_util.js';
import {constants} from '../../../common/constants.js';
import {Cursor, CURSOR_NODE_INDEX} from '../../../common/cursors/cursor.js';
import {CursorRange} from '../../../common/cursors/range.js';
import {LocalStorage} from '../../../common/local_storage.js';
import {Msgs} from '../../common/msgs.js';

import {OutputFormatParserObserver} from './output_format_parser.js';
import {OutputFormatTree} from './output_format_tree.js';
import {OutputInterface} from './output_interface.js';
import {OutputRoleInfo} from './output_role_info.js';
import * as outputTypes from './output_types.js';

const Dir = constants.Dir;
const NameFromType = chrome.automation.NameFromType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;

// TODO(anastasi): Move formatting logic to this class.
/** @implements {OutputFormatParserObserver} */
export class OutputFormatter {
  /**
   * @param {!OutputInterface} output
   * @param {!outputTypes.OutputFormattingData} params
   */
  constructor(output, params) {
    /** @private {outputTypes.OutputSpeechProperties|undefined} */
    this.speechProps_ = params.opt_speechProps;
    /** @private {!OutputInterface} */
    this.output_ = output;
    /** @private {!outputTypes.OutputFormattingData} */
    this.params_ = params;
  }

  /** @override */
  onTokenStart() {}

  /** @override */
  onNodeAttributeOrSpecialToken(token, tree, options) {
    if (this.output_.shouldSuppress(token)) {
      return true;
    }

    if (token === 'value') {
      this.formatValue_(this.params_, token, options);
    } else if (token === 'name') {
      this.formatName_(this.params_, token, options);
    } else if (token === 'description') {
      this.formatDescription_(this.params_, token, options);
    } else if (token === 'urlFilename') {
      this.formatUrlFilename_(this.params_, token, options);
    } else if (token === 'nameFromNode') {
      this.formatNameFromNode_(this.params_, token, options);
    } else if (token === 'nameOrDescendants') {
      // This token is similar to nameOrTextContent except it gathers
      // rich output for descendants. It also lets name from contents
      // override the descendants text if |node| has only static text
      // children.
      this.formatNameOrDescendants_(this.params_, token, options);
    } else if (token === 'indexInParent') {
      this.formatIndexInParent_(this.params_, token, tree, options);
    } else if (token === 'restriction') {
      this.formatRestriction_(this.params_, token);
    } else if (token === 'checked') {
      this.formatChecked_(this.params_, token);
    } else if (token === 'pressed') {
      this.formatPressed_(this.params_, token);
    } else if (token === 'state') {
      this.formatState_(this.params_, token);
    } else if (token === 'find') {
      this.formatFind_(this.params_, token, tree);
    } else if (token === 'descendants') {
      this.formatDescendants_(this.params_, token);
    } else if (token === 'joinedDescendants') {
      this.formatJoinedDescendants_(this.params_, token, options);
    } else if (token === 'role') {
      if (LocalStorage.get('useVerboseMode') === false) {
        return true;
      }
      if (this.output_.useAuralStyle) {
        this.speechProps_ = new outputTypes.OutputSpeechProperties();
        this.speechProps_.properties['relativePitch'] = -0.3;
      }

      this.formatRole_(this.params_, token, options);
    } else if (token === 'inputType') {
      this.formatInputType_(this.params_, token, options);
    } else if (
        token === 'tableCellRowIndex' || token === 'tableCellColumnIndex') {
      this.formatTableCellIndex_(this.params_, token, options);
    } else if (token === 'cellIndexText') {
      this.formatCellIndexText_(this.params_, token, options);
    } else if (token === 'node') {
      this.output_.formatNode_(this.params_, token, tree, options);
    } else if (token === 'nameOrTextContent' || token === 'textContent') {
      this.output_.formatTextContent_(this.params_, token, options);
    } else if (this.params_.node[token] !== undefined) {
      this.output_.formatAsFieldAccessor_(this.params_, token, options);
    } else if (outputTypes.OUTPUT_STATE_INFO[token]) {
      this.output_.formatAsStateValue_(this.params_, token, options);
    } else if (token === 'phoneticReading') {
      this.output_.formatPhoneticReading_(this.params_);
    } else if (token === 'listNestedLevel') {
      this.output_.formatListNestedLevel_(this.params_);
    } else if (token === 'precedingBullet') {
      this.output_.formatPrecedingBullet_(this.params_);
    } else if (tree.firstChild) {
      this.output_.formatCustomFunction_(this.params_, token, tree, options);
    }
  }

  /** @override */
  onMessageToken(token, tree, options) {
    this.params_.outputFormatLogger.write(' @');
    if (this.output_.useAuralStyle) {
      if (!this.speechProps_) {
        this.speechProps_ = new outputTypes.OutputSpeechProperties();
      }
      this.speechProps_.properties['relativePitch'] = -0.2;
    }
    this.output_.formatMessage_(this.params_, token, tree, options);
  }

  /** @override */
  onSpeechPropertyToken(token, tree, options) {
    this.params_.outputFormatLogger.write(' ! ' + token + '\n');
    this.speechProps_ = new outputTypes.OutputSpeechProperties();
    this.speechProps_.properties[token] = true;
    if (tree.firstChild) {
      if (!this.output_.useAuralStyle) {
        this.speechProps_ = undefined;
        return true;
      }

      let value = tree.firstChild.value;

      // Currently, speech params take either attributes or floats.
      let float = 0;
      if (float = parseFloat(value)) {
        value = float;
      } else {
        value = parseFloat(this.params_.node[value]) / -10.0;
      }
      this.speechProps_.properties[token] = value;
      return true;
    }
  }

  /** @override */
  onTokenEnd() {
    const buff = this.params_.outputBuffer;

    // Post processing.
    if (this.speechProps_) {
      if (buff.length > 0) {
        buff[buff.length - 1].setSpan(this.speechProps_, 0, 0);
        this.speechProps_ = null;
      }
    }
  }

  /**
   * @param {!OutputFormatTree} tree
   * @return {!Set}
   * @private
   */
  createRoles_(tree) {
    const roles = new Set();
    for (let currentNode = tree.firstChild; currentNode;
         currentNode = currentNode.nextSibling) {
      roles.add(currentNode.value);
    }
    return roles;
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatCellIndexText_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    if (node.htmlAttributes['aria-coltext']) {
      let value = node.htmlAttributes['aria-coltext'];
      let row = node;
      while (row && row.role !== RoleType.ROW) {
        row = row.parent;
      }
      if (!row || !row.htmlAttributes['aria-rowtext']) {
        return;
      }
      value += row.htmlAttributes['aria-rowtext'];
      this.output_.append_(buff, value, options);
      formatLog.writeTokenWithValue(token, value);
    } else {
      formatLog.write(token);
      this.output_.format_({
        node,
        outputFormat: ` @cell_summary($if($tableCellAriaRowIndex,
                  $tableCellAriaRowIndex, $tableCellRowIndex),
                $if($tableCellAriaColumnIndex, $tableCellAriaColumnIndex,
                  $tableCellColumnIndex))`,
        outputBuffer: buff,
        outputFormatLogger: formatLog,
      });
    }
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @private
   */
  formatChecked_(data, token) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    const msg = outputTypes.OutputPropertyMap.CHECKED[node.checked];
    if (msg) {
      formatLog.writeToken(token);
      this.output_.format_({
        node,
        outputFormat: '@' + msg,
        outputBuffer: buff,
        outputFormatLogger: formatLog,
      });
    }
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @private
   */
  formatDescendants_(data, token) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    if (!node) {
      return;
    }

    let leftmost = node;
    let rightmost = node;
    if (AutomationPredicate.leafOrStaticText(node)) {
      // Find any deeper leaves, if any, by starting from one level
      // down.
      leftmost = node.firstChild;
      rightmost = node.lastChild;
      if (!leftmost || !rightmost) {
        return;
      }
    }

    // Construct a range to the leftmost and rightmost leaves. This
    // range gets rendered below which results in output that is the
    // same as if a user navigated through the entire subtree of |node|.
    leftmost = AutomationUtil.findNodePre(
        leftmost, Dir.FORWARD, AutomationPredicate.leafOrStaticText);
    rightmost = AutomationUtil.findNodePre(
        rightmost, Dir.BACKWARD, AutomationPredicate.leafOrStaticText);
    if (!leftmost || !rightmost) {
      return;
    }

    const subrange = new CursorRange(
        new Cursor(leftmost, CURSOR_NODE_INDEX),
        new Cursor(rightmost, CURSOR_NODE_INDEX));
    let prev = null;
    if (node) {
      prev = CursorRange.fromNode(node);
    }
    formatLog.writeToken(token);
    this.output_.render_(
        subrange, prev, outputTypes.OutputCustomEvent.NAVIGATE, buff, formatLog,
        {suppressStartEndAncestry: true});
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatDescription_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    if (node.name === node.description) {
      return;
    }

    options.annotation.push(token);
    this.output_.append_(buff, node.description || '', options);
    formatLog.writeTokenWithValue(token, node.description);
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @private
   */
  formatFind_(data, token, tree) {
    const buff = data.outputBuffer;
    const formatLog = data.outputFormatLogger;
    let node = data.node;

    // Find takes two arguments: JSON query string and format string.
    if (tree.firstChild) {
      const jsonQuery = tree.firstChild.value;
      node = node.find(
          /** @type {chrome.automation.FindParams}*/ (JSON.parse(jsonQuery)));
      const formatString = tree.firstChild.nextSibling || '';
      if (node) {
        formatLog.writeToken(token);
        this.output_.format_({
          node,
          outputFormat: formatString,
          outputBuffer: buff,
          outputFormatLogger: formatLog,
        });
      }
    }
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatIndexInParent_(data, token, tree, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    if (node.parent) {
      options.annotation.push(token);
      let roles;
      if (tree.firstChild) {
        roles = this.createRoles_(tree);
      } else {
        roles = new Set();
        roles.add(node.role);
      }

      let count = 0;
      for (let i = 0, child; child = node.parent.children[i]; i++) {
        if (roles.has(child.role)) {
          count++;
        }
        if (node === child) {
          break;
        }
      }
      this.output_.append_(buff, String(count));
      formatLog.writeTokenWithValue(token, String(count));
    }
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatInputType_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    if (!node.inputType) {
      return;
    }
    options.annotation.push(token);
    let msgId =
        outputTypes.INPUT_TYPE_MESSAGE_IDS[node.inputType] || 'input_type_text';
    if (this.output_.formatAsBraille) {
      msgId = msgId + '_brl';
    }
    this.output_.append_(buff, Msgs.getMsg(msgId), options);
    formatLog.writeTokenWithValue(token, Msgs.getMsg(msgId));
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatJoinedDescendants_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    const unjoined = [];
    formatLog.write('joinedDescendants {');
    this.output_.format_({
      node,
      outputFormat: '$descendants',
      outputBuffer: unjoined,
      outputFormatLogger: formatLog,
    });
    this.output_.append_(buff, unjoined.join(' '), options);
    formatLog.write(
        '}: ' + (unjoined.length ? unjoined.join(' ') : 'EMPTY') + '\n');
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatName_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const prevNode = data.opt_prevNode;
    const formatLog = data.outputFormatLogger;

    options.annotation.push(token);
    const earcon = node ? this.output_.findEarcon_(node, prevNode) : null;
    if (earcon) {
      options.annotation.push(earcon);
    }

    // Place the selection on the first character of the name if the
    // node is the active descendant. This ensures the braille window is
    // panned appropriately.
    if (node.activeDescendantFor && node.activeDescendantFor.length > 0) {
      options.annotation.push(new outputTypes.OutputSelectionSpan(0, 0));
    }

    if (LocalStorage.get('languageSwitching')) {
      this.output_.assignLocaleAndAppend_(node.name || '', node, buff, options);
    } else {
      this.output_.append_(buff, node.name || '', options);
    }

    formatLog.writeTokenWithValue(token, node.name);
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatNameFromNode_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    if (node.nameFrom === NameFromType.CONTENTS) {
      return;
    }

    options.annotation.push('name');
    this.output_.append_(buff, node.name || '', options);
    formatLog.writeTokenWithValue(token, node.name);
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatNameOrDescendants_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    options.annotation.push(token);
    if (node.name &&
        (node.nameFrom !== NameFromType.CONTENTS ||
         node.children.every(child => child.role === RoleType.STATIC_TEXT))) {
      this.output_.append_(buff, node.name || '', options);
      formatLog.writeTokenWithValue(token, node.name);
    } else {
      formatLog.writeToken(token);
      this.output_.format_({
        node,
        outputFormat: '$descendants',
        outputBuffer: buff,
        outputFormatLogger: formatLog,
      });
    }
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @private
   */
  formatPressed_(data, token) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    const msg = outputTypes.OutputPropertyMap.PRESSED[node.checked];
    if (msg) {
      formatLog.writeToken(token);
      this.output_.format_({
        node,
        outputFormat: '@' + msg,
        outputBuffer: buff,
        outputFormatLogger: formatLog,
      });
    }
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @private
   */
  formatRestriction_(data, token) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    const msg = outputTypes.OutputPropertyMap.RESTRICTION[node.restriction];
    if (msg) {
      formatLog.writeToken(token);
      this.output_.format_({
        node,
        outputFormat: '@' + msg,
        outputBuffer: buff,
        outputFormatLogger: formatLog,
      });
    }
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatRole_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    options.annotation.push(token);
    let msg = node.role;
    const info = OutputRoleInfo[node.role];
    if (node.roleDescription) {
      msg = node.roleDescription;
    } else if (info) {
      if (this.output_.formatAsBraille) {
        msg = Msgs.getMsg(info.msgId + '_brl');
      } else if (info.msgId) {
        msg = Msgs.getMsg(info.msgId);
      }
    } else {
      // We can safely ignore this role. ChromeVox output tests cover
      // message id validity.
      return;
    }
    this.output_.append_(buff, msg || '', options);
    formatLog.writeTokenWithValue(token, msg);
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @private
   */
  formatState_(data, token) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    if (node.state) {
      Object.getOwnPropertyNames(node.state).forEach(state => {
        const stateInfo = outputTypes.OUTPUT_STATE_INFO[state];
        if (stateInfo && !stateInfo.isRoleSpecific && stateInfo.on) {
          formatLog.writeToken(token);
          this.output_.format_({
            node,
            outputFormat: '$' + state,
            outputBuffer: buff,
            outputFormatLogger: formatLog,
          });
        }
      });
    }
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatTableCellIndex_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    let value = node[token];
    if (value === undefined) {
      return;
    }
    value = String(value + 1);
    options.annotation.push(token);
    this.output_.append_(buff, value, options);
    formatLog.writeTokenWithValue(token, value);
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   */
  formatUrlFilename_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    options.annotation.push('name');
    const url = node.url || '';
    let filename = '';
    if (url.substring(0, 4) !== 'data') {
      filename = url.substring(url.lastIndexOf('/') + 1, url.lastIndexOf('.'));

      // Hack to not speak the filename if it's ridiculously long.
      if (filename.length >= 30) {
        filename = filename.substring(0, 16) + '...';
      }
    }
    this.output_.append_(buff, filename, options);
    formatLog.writeTokenWithValue(token, filename);
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatValue_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    const text = node.value || '';
    if (!node.state[StateType.EDITABLE] && node.name === text) {
      return;
    }

    let selectedText = '';
    if (node.textSelStart !== undefined) {
      options.annotation.push(new outputTypes.OutputSelectionSpan(
          node.textSelStart || 0, node.textSelEnd || 0));

      if (node.value) {
        selectedText =
            node.value.substring(node.textSelStart || 0, node.textSelEnd || 0);
      }
    }
    options.annotation.push(token);
    if (selectedText && !this.output_.formatAsBraille &&
        node.state[StateType.FOCUSED]) {
      this.output_.append_(buff, selectedText, options);
      this.output_.append_(buff, Msgs.getMsg('selected'));
      formatLog.writeTokenWithValue(token, selectedText);
      formatLog.write('selected\n');
    } else {
      this.output_.append_(buff, text, options);
      formatLog.writeTokenWithValue(token, text);
    }
  }
}
