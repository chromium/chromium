// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SACache} from './cache.js';
import {Navigator} from './navigator.js';
import {DesktopNode} from './nodes/desktop_node.js';
import {SAChildNode, SARootNode} from './nodes/switch_access_node.js';
import {SwitchAccessPredicate} from './switch_access_predicate.js';

const AutomationNode = chrome.automation.AutomationNode;

/** This class is a structure to store previous state for restoration. */
export class FocusData {
  /**
   * @param {!SARootNode} group
   * @param {!SAChildNode} focus Must be a child of |group|.
   */
  constructor(group, focus) {
    /** @type {!SARootNode} */
    this.group = group;
    /** @type {!SAChildNode} */
    this.focus = focus;
  }

  /** @return {boolean} */
  isValid() {
    if (this.group.isValidGroup()) {
      // Ensure it is still valid. Some nodes may have been added
      // or removed since this was last used.
      this.group.refreshChildren();
    }
    return this.group.isValidGroup();
  }
}

/** This class handles saving and retrieving FocusData. */
export class FocusHistory {
  constructor() {
    /** @private {!Array<!FocusData>} */
    this.dataStack_ = [];
  }

  /**
   * Creates the restore data to get from the desktop node to the specified
   * automation node.
   * Erases the current history and replaces with the new data.
   * @param {!AutomationNode} node
   * @return {boolean} Whether the history was rebuilt from the given node.
   */
  buildFromAutomationNode(node) {
    if (!node.parent) {
      // No ancestors, cannot create stack.
      return false;
    }
    const cache = new SACache();
    // Create a list of ancestors.
    const ancestorStack = [node];
    while (node.parent) {
      ancestorStack.push(node.parent);
      node = node.parent;
    }

    let group = DesktopNode.build(ancestorStack.pop());
    const firstAncestor = ancestorStack[ancestorStack.length - 1];
    if (!SwitchAccessPredicate.isInterestingSubtree(firstAncestor, cache)) {
      // If the topmost ancestor (other than the desktop) is entirely
      // uninteresting, we leave the history as is.
      return false;
    }

    const newDataStack = [];
    while (ancestorStack.length > 0) {
      const candidate = ancestorStack.pop();
      if (!SwitchAccessPredicate.isInteresting(candidate, group, cache)) {
        continue;
      }

      const focus = group.findChild(candidate);
      if (!focus) {
        continue;
      }
      newDataStack.push(new FocusData(group, focus));

      group = focus.asRootNode();
      if (!group) {
        break;
      }
    }

    if (newDataStack.length === 0) {
      return false;
    }
    this.dataStack_ = newDataStack;
    return true;
  }

  /**
   * @param {!function(!FocusData): boolean} predicate
   * @return {boolean}
   */
  containsDataMatchingPredicate(predicate) {
    for (const data of this.dataStack_) {
      if (predicate(data)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns the most proximal restore data, but does not remove it from the
   * history.
   * @return {?FocusData}
   */
  peek() {
    return this.dataStack_[this.dataStack_.length - 1] || null;
  }

  /** @return {!FocusData} */
  retrieve() {
    let data = this.dataStack_.pop();
    while (data && !data.isValid()) {
      data = this.dataStack_.pop();
    }

    if (data) {
      return data;
    }

    // If we don't have any valid history entries, fallback to the desktop node.
    const desktop = new DesktopNode(Navigator.byItem.desktopNode);
    return new FocusData(desktop, desktop.firstChild);
  }

  /** @param {!FocusData} data */
  save(data) {
    this.dataStack_.push(data);
  }

  /** Support for this type being used in for..of loops. */
  [Symbol.iterator]() {
    return this.dataStack_[Symbol.iterator]();
  }
}
