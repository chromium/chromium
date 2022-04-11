// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Dumps a log of the accessibility tree.
 */

goog.provide('SimpleAutomationNode');
goog.provide('TreeDumper');

const AutomationNode = chrome.automation.AutomationNode;

SimpleAutomationNode = class {
  /**
   * @param {!AutomationNode} node
   */
  constructor(node) {
    this.name = node.name;
    this.role = node.role;
    this.value = node.value;
    this.url = node.url;
    /** Object Rect must be copied in the different pointer. */
    this.location = Object.assign({}, node.location);
    this.children = [];
    for (let i = 0; i < node.children.length; i++) {
      this.children.push(new SimpleAutomationNode(node.children[i]));
    }

    /** @type {string} */
    this.logStr = '';

    /** @return {string} */
    this.toString = function() {
      if (this.logStr.length) {
        return this.logStr;
      }

      if (node.name) {
        this.logStr += 'name=' + node.name + ' ';
      }
      if (node.role) {
        this.logStr += 'role=' + node.role + ' ';
      }
      if (node.value) {
        this.logStr += 'value=' + node.value + ' ';
      }
      if (node.location) {
        this.logStr +=
            'location=(' + node.location.left + ', ' + node.location.top + ') ';
        this.logStr +=
            'size=(' + node.location.width + ', ' + node.location.height + ') ';
      }
      if (node.url) {
        this.logStr += 'url=' + node.url + ' ';
      }
      return this.logStr;
    };
  }
};

/**
 * Structure of accessibility tree.
 * This constructor will traverse whole tree to save the tree structure.
 * This should only be called when the user intended to do so.
 */
TreeDumper = class {
  /**
   * @param {!AutomationNode} root
   */
  constructor(root) {
    /**
     * @type {!SimpleAutomationNode}
     */
    this.rootNode = new SimpleAutomationNode(root);

    /**
     * @type {string}
     * @private
     */
    this.treeStr_;
  }

  /** @return {string} */
  treeToString() {
    if (!this.treeStr_) {
      this.treeStr_ = this.formatTree_();
    }

    return this.treeStr_;
  }

  /**
   * @param {!SimpleAutomationNode} node
   * @param {number} rank
   * @private
   */
  createTreeRecursive_(node, rank) {
    let nodeStr = '';
    nodeStr += '++'.repeat(rank);
    nodeStr += node.toString();
    nodeStr += '\n';

    for (let i = 0; i < node.children.length; i++) {
      const nextNode = node.children[i];
      nodeStr += this.createTreeRecursive_(nextNode, rank + 1);
    }
    return nodeStr;
  }

  /**
   * @return {string}
   * @private
   * */
  formatTree_() {
    const treeStr = this.createTreeRecursive_(this.rootNode, 0);
    return treeStr;
  }
};
