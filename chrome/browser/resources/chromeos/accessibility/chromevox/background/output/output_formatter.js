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
import {AutomationTreeWalker} from '../../../common/tree_walker.js';
import {EarconId} from '../../common/earcon_id.js';
import {Msgs} from '../../common/msgs.js';
import {PhoneticData} from '../phonetic_data.js';

import {OutputFormatParser, OutputFormatParserObserver} from './output_format_parser.js';
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

  /**
   * Format the node given the format specifier.
   * @param {!OutputInterface} output
   * @param {!outputTypes.OutputFormattingData} params All the required and
   *     optional parameters for formatting.
   */
  static format(output, params) {
    const formatter = new OutputFormatter(output, params);
    new OutputFormatParser(formatter).parse(params.outputFormat);
  }

  /** @override */
  onTokenStart() {}

  /** @override */
  onNodeAttributeOrSpecialToken(token, tree, options) {
    if (this.output_.shouldSuppress(token)) {
      return true;
    }

    if (token === 'cellIndexText') {
      this.formatCellIndexText_(this.params_, token, options);
    } else if (token === 'checked') {
      this.formatChecked_(this.params_, token);
    } else if (token === 'descendants') {
      this.formatDescendants_(this.params_, token);
    } else if (token === 'description') {
      this.formatDescription_(this.params_, token, options);
    } else if (token === 'find') {
      this.formatFind_(this.params_, token, tree);
    } else if (token === 'inputType') {
      this.formatInputType_(this.params_, token, options);
    } else if (token === 'indexInParent') {
      this.formatIndexInParent_(this.params_, token, tree, options);
    } else if (token === 'joinedDescendants') {
      this.formatJoinedDescendants_(this.params_, token, options);
    } else if (token === 'listNestedLevel') {
      this.formatListNestedLevel_(this.params_);
    } else if (token === 'name') {
      this.formatName_(this.params_, token, options);
    } else if (token === 'nameFromNode') {
      this.formatNameFromNode_(this.params_, token, options);
    } else if (token === 'nameOrDescendants') {
      // This token is similar to nameOrTextContent except it gathers
      // rich output for descendants. It also lets name from contents
      // override the descendants text if |node| has only static text
      // children.
      this.formatNameOrDescendants_(this.params_, token, options);
    } else if (token === 'nameOrTextContent' || token === 'textContent') {
      this.formatTextContent_(this.params_, token, options);
    } else if (token === 'node') {
      this.formatNode_(this.params_, token, tree, options);
    } else if (token === 'phoneticReading') {
      this.formatPhoneticReading_(this.params_);
    } else if (token === 'precedingBullet') {
      this.formatPrecedingBullet_(this.params_);
    } else if (token === 'pressed') {
      this.formatPressed_(this.params_, token);
    } else if (token === 'restriction') {
      this.formatRestriction_(this.params_, token);
    } else if (token === 'role') {
      if (LocalStorage.get('useVerboseMode') === false) {
        return true;
      }
      if (this.output_.useAuralStyle) {
        this.speechProps_ = new outputTypes.OutputSpeechProperties();
        this.speechProps_.properties['relativePitch'] = -0.3;
      }

      this.formatRole_(this.params_, token, options);
    } else if (token === 'state') {
      this.formatState_(this.params_, token);
    } else if (
        token === 'tableCellRowIndex' || token === 'tableCellColumnIndex') {
      this.formatTableCellIndex_(this.params_, token, options);
    } else if (token === 'urlFilename') {
      this.formatUrlFilename_(this.params_, token, options);
    } else if (token === 'value') {
      this.formatValue_(this.params_, token, options);
    } else if (this.params_.node[token] !== undefined) {
      this.formatAsFieldAccessor_(this.params_, token, options);
    } else if (outputTypes.OUTPUT_STATE_INFO[token]) {
      this.formatAsStateValue_(this.params_, token, options);
    } else if (tree.firstChild) {
      this.formatCustomFunction_(this.params_, token, tree, options);
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
    this.formatMessage_(this.params_, token, tree, options);
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
  formatAsFieldAccessor_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    options.annotation.push(token);
    let value = node[token];
    if (typeof value === 'number') {
      value = String(value);
    }
    this.output_.append(buff, value, options);
    formatLog.writeTokenWithValue(token, value);
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatAsStateValue_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    options.annotation.push('state');
    const stateInfo = outputTypes.OUTPUT_STATE_INFO[token];
    let resolvedInfo = {};
    resolvedInfo = node.state[/** @type {StateType} */ (token)] ? stateInfo.on :
                                                                  stateInfo.off;
    if (!resolvedInfo) {
      return;
    }
    if (this.output_.formatAsSpeech && resolvedInfo.earcon) {
      options.annotation.push(
          new outputTypes.OutputEarconAction(resolvedInfo.earcon),
          node.location || undefined);
    }
    const msgId = this.output_.formatAsBraille ? resolvedInfo.msgId + '_brl' :
                                                 resolvedInfo.msgId;
    const msg = Msgs.getMsg(msgId);
    this.output_.append(buff, msg, options);
    formatLog.writeTokenWithValue(token, msg);
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
      this.output_.append(buff, value, options);
      formatLog.writeTokenWithValue(token, value);
    } else {
      formatLog.write(token);
      OutputFormatter.format(this.output_, {
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
      OutputFormatter.format(this.output_, {
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
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatCustomFunction_(data, token, tree, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    // Custom functions.
    if (token === 'if') {
      formatLog.writeToken(token);
      const cond = tree.firstChild;
      const attrib = cond.value.slice(1);
      if (AutomationUtil.isTruthy(node, attrib)) {
        formatLog.write(attrib + '==true => ');
        OutputFormatter.format(this.output_, {
          node,
          outputFormat: cond.nextSibling || '',
          outputBuffer: buff,
          outputFormatLogger: formatLog,
        });
      } else if (AutomationUtil.isFalsey(node, attrib)) {
        formatLog.write(attrib + '==false => ');
        OutputFormatter.format(this.output_, {
          node,
          outputFormat: cond.nextSibling.nextSibling || '',
          outputBuffer: buff,
          outputFormatLogger: formatLog,
        });
      }
    } else if (token === 'nif') {
      formatLog.writeToken(token);
      const cond = tree.firstChild;
      const attrib = cond.value.slice(1);
      if (AutomationUtil.isFalsey(node, attrib)) {
        formatLog.write(attrib + '==false => ');
        OutputFormatter.format(this.output_, {
          node,
          outputFormat: cond.nextSibling || '',
          outputBuffer: buff,
          outputFormatLogger: formatLog,
        });
      } else if (AutomationUtil.isTruthy(node, attrib)) {
        formatLog.write(attrib + '==true => ');
        OutputFormatter.format(this.output_, {
          node,
          outputFormat: cond.nextSibling.nextSibling || '',
          outputBuffer: buff,
          outputFormatLogger: formatLog,
        });
      }
    } else if (token === 'earcon') {
      // Ignore unless we're generating speech output.
      if (!this.output_.formatAsSpeech) {
        return;
      }

      options.annotation.push(new outputTypes.OutputEarconAction(
          EarconId[tree.firstChild.value], node.location || undefined));
      this.output_.append(buff, '', options);
      formatLog.writeTokenWithValue(token, tree.firstChild.value);
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
    this.output_.render(
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
    this.output_.append(buff, node.description || '', options);
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
        OutputFormatter.format(this.output_, {
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
      this.output_.append(buff, String(count));
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
    this.output_.append(buff, Msgs.getMsg(msgId), options);
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
    OutputFormatter.format(this.output_, {
      node,
      outputFormat: '$descendants',
      outputBuffer: unjoined,
      outputFormatLogger: formatLog,
    });
    this.output_.append(buff, unjoined.join(' '), options);
    formatLog.write(
        '}: ' + (unjoined.length ? unjoined.join(' ') : 'EMPTY') + '\n');
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @private
   */
  formatListNestedLevel_(data) {
    const buff = data.outputBuffer;
    const node = data.node;

    let level = 0;
    let current = node;
    while (current) {
      if (current.role === RoleType.LIST) {
        level += 1;
      }
      current = current.parent;
    }
    this.output_.append(buff, level.toString());
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatMessage_(data, token, tree, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    const isPluralized = (token[0] === '@');
    if (isPluralized) {
      token = token.slice(1);
    }
    // Tokens can have substitutions.
    const pieces = token.split('+');
    token = pieces.reduce((prev, cur) => {
      let lookup = cur;
      if (cur[0] === '$') {
        lookup = node[cur.slice(1)];
      }
      return prev + lookup;
    }, '');
    const msgId = token;
    let msgArgs = [];
    formatLog.write(token + '{');
    if (!isPluralized) {
      let curArg = tree.firstChild;
      while (curArg) {
        if (curArg.value[0] !== '$') {
          const errorMsg = 'Unexpected value: ' + curArg.value;
          formatLog.writeError(errorMsg);
          console.error(errorMsg);
          return;
        }
        let msgBuff = [];
        OutputFormatter.format(this.output_, {
          node,
          outputFormat: curArg,
          outputBuffer: msgBuff,
          outputFormatLogger: formatLog,
        });
        // Fill in empty string if nothing was formatted.
        if (!msgBuff.length) {
          msgBuff = [''];
        }
        msgArgs = msgArgs.concat(msgBuff);
        curArg = curArg.nextSibling;
      }
    }
    let msg = Msgs.getMsg(msgId, msgArgs);
    try {
      if (this.output_.formatAsBraille) {
        msg = Msgs.getMsg(msgId + '_brl', msgArgs) || msg;
      }
    } catch (e) {
      // TODO(accessibility): Handle whatever error this is.
    }

    if (!msg) {
      const errorMsg = 'Could not get message ' + msgId;
      formatLog.writeError(errorMsg);
      console.error(errorMsg);
      return;
    }

    if (isPluralized) {
      const arg = tree.firstChild;
      if (!arg || arg.nextSibling) {
        const errorMsg = 'Pluralized messages take exactly one argument';
        formatLog.writeError(errorMsg);
        console.error(errorMsg);
        return;
      }
      if (arg.value[0] !== '$') {
        const errorMsg = 'Unexpected value: ' + arg.value;
        formatLog.writeError(errorMsg);
        console.error(errorMsg);
        return;
      }
      const argBuff = [];
      OutputFormatter.format(this.output_, {
        node,
        outputFormat: arg,
        outputBuffer: argBuff,
        outputFormatLogger: formatLog,
      });
      const namedArgs = {COUNT: Number(argBuff[0])};
      msg = new goog.i18n.MessageFormat(msg).format(namedArgs);
    }
    formatLog.write('}');

    this.output_.append(buff, msg, options);
    formatLog.write(': ' + msg + '\n');
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
    const earcon = node ? this.output_.findEarcon(node, prevNode) : null;
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
      this.output_.assignLocaleAndAppend(node.name || '', node, buff, options);
    } else {
      this.output_.append(buff, node.name || '', options);
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
    this.output_.append(buff, node.name || '', options);
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
      this.output_.append(buff, node.name || '', options);
      formatLog.writeTokenWithValue(token, node.name);
    } else {
      formatLog.writeToken(token);
      OutputFormatter.format(this.output_, {
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
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatNode_(data, token, tree, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;
    let prevNode = data.opt_prevNode;

    if (!tree.firstChild) {
      return;
    }

    const relationName = tree.firstChild.value;
    if (relationName === 'tableCellColumnHeaders') {
      // Skip output when previous position falls on the same column.
      while (prevNode && !AutomationPredicate.cellLike(prevNode)) {
        prevNode = prevNode.parent;
      }
      if (prevNode &&
          prevNode.tableCellColumnIndex === node.tableCellColumnIndex) {
        return;
      }

      const headers = node.tableCellColumnHeaders;
      if (headers) {
        for (let i = 0; i < headers.length; i++) {
          const header = headers[i].name;
          if (header) {
            this.output_.append(buff, header, options);
            formatLog.writeTokenWithValue(token, header);
          }
        }
      }
    } else if (relationName === 'tableCellRowHeaders') {
      const headers = node.tableCellRowHeaders;
      if (headers) {
        for (let i = 0; i < headers.length; i++) {
          const header = headers[i].name;
          if (header) {
            this.output_.append(buff, header, options);
            formatLog.writeTokenWithValue(token, header);
          }
        }
      }
    } else if (node[relationName]) {
      const related = node[relationName];
      this.output_.formatNode(
          related, related, outputTypes.OutputCustomEvent.NAVIGATE, buff,
          formatLog);
    }
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @private
   */
  formatPhoneticReading_(data) {
    const buff = data.outputBuffer;
    const node = data.node;

    const text =
        PhoneticData.forText(node.name || '', chrome.i18n.getUILanguage());
    this.output_.append(buff, text);
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @private
   */
  formatPrecedingBullet_(data) {
    const buff = data.outputBuffer;
    const node = data.node;

    let current = node;
    if (current.role === RoleType.INLINE_TEXT_BOX) {
      current = current.parent;
    }
    if (!current || current.role !== RoleType.STATIC_TEXT) {
      return;
    }
    current = current.previousSibling;
    if (current && current.role === RoleType.LIST_MARKER) {
      this.output_.append(buff, current.name || '');
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
      OutputFormatter.format(this.output_, {
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
      OutputFormatter.format(this.output_, {
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
    this.output_.append(buff, msg || '', options);
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
          OutputFormatter.format(this.output_, {
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
    this.output_.append(buff, value, options);
    formatLog.writeTokenWithValue(token, value);
  }

  /**
   * @param {!outputTypes.OutputFormattingData} data
   * @param {string} token
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @private
   */
  formatTextContent_(data, token, options) {
    const buff = data.outputBuffer;
    const node = data.node;
    const formatLog = data.outputFormatLogger;

    if (node.name && token === 'nameOrTextContent') {
      formatLog.writeToken(token);
      OutputFormatter.format(this.output_, {
        node,
        outputFormat: '$name',
        outputBuffer: buff,
        outputFormatLogger: formatLog,
      });
      return;
    }

    if (!node.firstChild) {
      return;
    }

    const root = node;
    const walker = new AutomationTreeWalker(node, Dir.FORWARD, {
      visit: AutomationPredicate.leafOrStaticText,
      leaf: n => {
        // The root might be a leaf itself, but we still want to descend
        // into it.
        return n !== root && AutomationPredicate.leafOrStaticText(n);
      },
      root: r => r === root,
    });
    const outputStrings = [];
    while (walker.next().node) {
      if (walker.node.name) {
        outputStrings.push(walker.node.name.trim());
      }
    }
    const finalOutput = outputStrings.join(' ');
    this.output_.append(buff, finalOutput, options);
    formatLog.writeTokenWithValue(token, finalOutput);
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
    this.output_.append(buff, filename, options);
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
      this.output_.append(buff, selectedText, options);
      this.output_.append(buff, Msgs.getMsg('selected'));
      formatLog.writeTokenWithValue(token, selectedText);
      formatLog.write('selected\n');
    } else {
      this.output_.append(buff, text, options);
      formatLog.writeTokenWithValue(token, text);
    }
  }
}
