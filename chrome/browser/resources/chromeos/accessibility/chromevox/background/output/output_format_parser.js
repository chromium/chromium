// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a push parser for Output format rules.
 */
import {OutputFormatTree} from './output_format_tree.js';

/**
 * Implemented by objects that wish to observe tokens from parsing Output format
 * rules.
 * @interface
 */
export class OutputFormatParserObserver {
  /**
   * Indicates the parse start of a new token.
   * @param {string} token
   * @return {boolean|undefined} True to skip to the next token.
   */
  onTokenStart(token) {}

  /**
   * Indicates a node attribute or special token (see output.js).
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @return {boolean|undefined} True to skip to the next token.
   */
  onNodeAttributeOrSpecialToken(token, tree, options) {}

  /**
   * Indicates a message token.
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @return {boolean|undefined} True to skip to the next token.
   */
  onMessageToken(token, tree, options) {}

  /**
   * Indicates a speech property token.
   * @param {string} token
   * @param {!OutputFormatTree} tree
   * @param {!{annotation: Array<*>, isUnique: (boolean|undefined)}} options
   * @return {boolean|undefined} True to skip to the next token.
   */
  onSpeechPropertyToken(token, tree, options) {}

  /**
   * Indicates the parse end of a new token.
   * @return {boolean|undefined} True to skip to the next token.
   */
  onTokenEnd() {}
}

export class OutputFormatParser {
  /** @param {!OutputFormatParserObserver} observer */
  constructor(observer) {
    /** @private {!OutputFormatParserObserver} observer */
    this.observer_ = observer;
  }

  /**
   * Starts parsing the given output format.
   * @param {string|!OutputFormatTree} format
   */
  parse(format) {
    const formatTrees = OutputFormatTree.parseFormat(format);
    formatTrees.forEach(tree => {
      // Obtain the operator token.
      let token = tree.value;

      // Set suffix options.
      const options = {};
      options.annotation = [];
      options.isUnique = token[token.length - 1] === '=';
      if (options.isUnique) {
        token = token.substring(0, token.length - 1);
      }

      // Process token based on prefix.
      const prefix = token[0];
      token = token.slice(1);

      if (this.observer_.onTokenStart(token)) {
        return;
      }

      // All possible tokens based on prefix.
      let skipToNextToken = false;
      if (prefix === '$') {
        skipToNextToken =
            this.observer_.onNodeAttributeOrSpecialToken(token, tree, options);
      } else if (prefix === '@') {
        skipToNextToken = this.observer_.onMessageToken(token, tree, options);
      } else if (prefix === '!') {
        skipToNextToken =
            this.observer_.onSpeechPropertyToken(token, tree, options);
      }

      if (skipToNextToken) {
        return;
      }

      this.observer_.onTokenEnd();
    });
  }
}
