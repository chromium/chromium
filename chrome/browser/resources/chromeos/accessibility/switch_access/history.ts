// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SACache} from './cache.js';
import {Navigator} from './navigator.js';
import {DesktopNode} from './nodes/desktop_node.js';
import {SAChildNode, SARootNode} from './nodes/switch_access_node.js';
import {SwitchAccessPredicate} from './switch_access_predicate.js';

type AutomationNode = chrome.automation.AutomationNode;

/** This class is a structure to store previous state for restoration. */
export class FocusData {
  group: SARootNode;
  focus: SAChildNode;

  /** |focus| Must be a child of |group|. */
  constructor(group: SARootNode, focus: SAChildNode) {
    this.group = group;
    this.focus = focus;
  }

  isValid(): boolean {
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
  private dataStack: FocusData[] = [];

  /**
   * Creates the restore data to get from the desktop node to the specified
   * automation node.
   * Erases the current history and replaces with the new data.
   * @return Whether the history was rebuilt from the given node.
   */
  buildFromAutomationNode(node: AutomationNode): boolean {
    if (!node.parent) {
      // No ancestors, cannot create stack.
      return false;
    }
    const cache = new SACache();
    // Create a list of ancestors.
    const ancestorStack: AutomationNode[] = [node];
    while (node.parent) {
      ancestorStack.push(node.parent);
      node = node.parent;
    }

    // TODO(b/314203187): Not null asserted, check that this is correct.
    let group: SARootNode = DesktopNode.build(ancestorStack.pop()!);
    const firstAncestor = ancestorStack[ancestorStack.length - 1];
    if (!SwitchAccessPredicate.isInterestingSubtree(firstAncestor, cache)) {
      // If the topmost ancestor (other than the desktop) is entirely
      // uninteresting, we leave the history as is.
      return false;
    }

    const newDataStack: FocusData[] = [];
    while (ancestorStack.length > 0) {
      const candidate = ancestorStack.pop();
      if (!SwitchAccessPredicate.isInteresting(candidate, group, cache)) {
        continue;
      }

      // TODO(b/314203187): Not null asserted, check that this is correct.
      const focus = group.findChild(candidate!);
      if (!focus) {
        continue;
      }
      newDataStack.push(new FocusData(group, focus));

      // TODO(b/314203187): Not null asserted, check that this is correct.
      group = focus.asRootNode()!;
      if (!group) {
        break;
      }
    }

    if (newDataStack.length === 0) {
      return false;
    }
    this.dataStack = newDataStack;
    return true;
  }

  containsDataMatchingPredicate(
    predicate: (data: FocusData) => boolean): boolean {
    for (const data of this.dataStack) {
      if (predicate(data)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns the most proximal restore data, but does not remove it from the
   * history.
   */
  peek(): FocusData | null {
    return this.dataStack[this.dataStack.length - 1] || null;
  }

  retrieve(): FocusData {
    let data = this.dataStack.pop();
    while (data && !data.isValid()) {
      data = this.dataStack.pop();
    }

    if (data) {
      return data;
    }

    // If we don't have any valid history entries, fallback to the desktop node.
    const desktop = new DesktopNode(Navigator.byItem.desktopNode);
    return new FocusData(desktop, desktop.firstChild);
  }

  save(data: FocusData): void {
    this.dataStack.push(data);
  }

  /** Support for this type being used in for..of loops. */
  [Symbol.iterator](): IterableIterator<FocusData> {
    return this.dataStack[Symbol.iterator]();
  }
}
