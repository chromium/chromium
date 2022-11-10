// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class that formats the parsed output tree.
 */
import {Msgs} from '../../common/msgs.js';

import {OutputFormatParserObserver} from './output_format_parser.js';
import {OutputInterface} from './output_interface.js';
import * as outputTypes from './output_types.js';

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
      this.output_.formatIndexInParent_(this.params_, token, tree, options);
    } else if (token === 'restriction') {
      this.output_.formatRestriction_(this.params_, token);
    } else if (token === 'checked') {
      this.output_.formatChecked_(this.params_, token);
    } else if (token === 'pressed') {
      this.output_.formatPressed_(this.params_, token);
    } else if (token === 'state') {
      this.output_.formatState_(this.params_, token);
    } else if (token === 'find') {
      this.output_.formatFind_(this.params_, token, tree);
    } else if (token === 'descendants') {
      this.output_.formatDescendants_(this.params_, token);
    } else if (token === 'joinedDescendants') {
      this.output_.formatJoinedDescendants_(this.params_, token, options);
    } else if (token === 'role') {
      if (localStorage['useVerboseMode'] === String(false)) {
        return true;
      }
      if (this.output_.useAuralStyle) {
        this.speechProps_ = new outputTypes.OutputSpeechProperties();
        this.speechProps_.properties['relativePitch'] = -0.3;
      }

      this.output_.formatRole_(this.params_, token, options);
    } else if (token === 'inputType') {
      this.output_.formatInputType_(this.params_, token, options);
    } else if (
        token === 'tableCellRowIndex' || token === 'tableCellColumnIndex') {
      this.output_.formatTableCellIndex_(this.params_, token, options);
    } else if (token === 'cellIndexText') {
      this.output_.formatCellIndexText_(this.params_, token, options);
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

    if (localStorage['languageSwitching'] === 'true') {
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
