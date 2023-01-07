// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Parses the output format.
 */

export class OutputFormatTree {
  /** @private */
  constructor() {
    /** @public {string} */
    this.value = '';
    /** @public {OutputFormatTree|undefined} */
    this.firstChild;
    /** @public {OutputFormatTree|undefined} */
    this.nextSibling;
    /** @public {OutputFormatTree|undefined} */
    this.parent;
  }

  /**
   * @param {string|!OutputFormatTree} format
   * @return {!Array<!OutputFormatTree>}
   */
  static parseFormat(format) {
    let formatTrees = [];
    // Hacky way to support args.
    if (typeof (format) === 'string') {
      format = format.replace(/([,:])\s+/gm, '$1');
      const words = format.split(' ');
      // Ignore empty strings.
      words.filter(word => Boolean(word));

      formatTrees = words.map(word => OutputFormatTree.buildFromString_(word));
    } else if (format) {
      formatTrees = [format];
    }
    return formatTrees;
  }

  /**
   * Parses the token containing a custom function and returns a tree.
   * @param {string} inputStr
   * @return {!OutputFormatTree}
   * @private
   */
  static buildFromString_(inputStr) {
    const root = new OutputFormatTree();
    let currentNode = root;
    let index = 0;
    let braceNesting = 0;
    while (index < inputStr.length) {
      if (inputStr[index] === '(') {
        currentNode.firstChild = new OutputFormatTree();
        currentNode.firstChild.parent = currentNode;
        currentNode = currentNode.firstChild;
      } else if (inputStr[index] === ')') {
        currentNode = currentNode.parent;
      } else if (inputStr[index] === '{') {
        braceNesting++;
        currentNode.value += inputStr[index];
      } else if (inputStr[index] === '}') {
        braceNesting--;
        currentNode.value += inputStr[index];
      } else if (inputStr[index] === ',' && braceNesting === 0) {
        currentNode.nextSibling = new OutputFormatTree();
        currentNode.nextSibling.parent = currentNode.parent;
        currentNode = currentNode.nextSibling;
      } else if (inputStr[index] === ' ' || inputStr[index] === '\n') {
        // Ignored.
      } else {
        currentNode.value += inputStr[index];
      }
      index++;
    }

    if (currentNode !== root) {
      throw 'Unbalanced parenthesis: ' + inputStr;
    }

    return root;
  }
}
