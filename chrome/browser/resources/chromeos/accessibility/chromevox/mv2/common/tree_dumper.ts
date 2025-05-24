// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Dumps a log of the accessibility tree.
 */

type AutomationNode = chrome.automation.AutomationNode;
type Rect = chrome.automation.Rect;

class SimpleAutomationNode {
  children: SimpleAutomationNode[] = [];
  location: Rect;
  logStr: string = '';
  name?: string;
  role?: string;
  url?: string;
  value?: string;

  constructor(node: AutomationNode) {
    this.name = node.name;
    this.role = node.role;
    this.value = node.value;
    this.url = node.url;
    /** Object Rect must be copied in the different pointer. */
    this.location = Object.assign({}, node.location);

    for (let i = 0; i < node.children.length; i++) {
      this.children.push(new SimpleAutomationNode(node.children[i]));
    }
  }

  toString(): string {
    if (this.logStr.length) {
      return this.logStr;
    }

    if (this.name) {
      this.logStr += 'name=' + this.name + ' ';
    }
    if (this.role) {
      this.logStr += 'role=' + this.role + ' ';
    }
    if (this.value) {
      this.logStr += 'value=' + this.value + ' ';
    }
    if (this.location) {
      this.logStr +=
          'location=(' + this.location.left + ', ' + this.location.top + ') ';
      this.logStr +=
          'size=(' + this.location.width + ', ' + this.location.height + ') ';
    }
    if (this.url) {
      this.logStr += 'url=' + this.url + ' ';
    }
    return this.logStr;
  }
}

/**
 * Structure of accessibility tree.
 * This constructor will traverse whole tree to save the tree structure.
 * This should only be called when the user intended to do so.
 */
export class TreeDumper {
  rootNode: SimpleAutomationNode;

  private treeStr_: string = '';

  constructor(root: AutomationNode) {
    this.rootNode = new SimpleAutomationNode(root);
  }

  treeToString(): string {
    if (!this.treeStr_) {
      this.treeStr_ = this.formatTree_();
    }

    return this.treeStr_;
  }

  private createTreeRecursive_(node: SimpleAutomationNode, rank: number)
      : string {
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

  private formatTree_(): string {
    const treeStr = this.createTreeRecursive_(this.rootNode, 0);
    return treeStr;
  }
}
