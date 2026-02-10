// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Container, Node, SplitTabVisualData, Tab, TabGroupVisualData, WindowNode} from './tab_strip_internals.mojom-webui.js';

/**
 * Model layer: Represents a UI node used by the ViewModel to build a semantic
 * hierarchy of tab structures.
 */
export interface ModelNode {
  /**
   * Unique path identifier for this node within the tab hierarchy
   * (e.g.'Container.windows[0].tabs[1]').
   */
  path: string;
  /** Reference to the raw Mojo data. */
  value: unknown;
  /** List of child nodes. Empty if no children. */
  children: ModelNode[];
  /**
   * Label for display in the tree view. It might contain additional
   * metadata such as group color, split layout, etc. for group or split nodes.
   */
  label: string;
  /**
   * A concise display name for the breadcrumb view. This is a concise version
   * of the label that skips additional metadata for group and split nodes.
   */
  displayName: string;
}

/**
 * Adapter that converts Mojo-provided data into a tree of ModelNodes suitable
 * for rendering (simplified view with semantic meaning).
 */
export class DataModelAdapter {
  /**
   * Build a tree of ModelNode from the given Container.
   */
  static build(container: Container): ModelNode {
    const root: ModelNode = {
      path: 'Container',
      value: container,
      children: [],
      label: 'Container',
      displayName: 'Container',
    };

    // TODO(crbug.com/427204855): Add adapter logic to map TabRestore,
    // SavedSession and RestoredSession data.
    const tabstripNode = this.buildTabStripTree(container.tabstripTree);

    root.children.push(tabstripNode);
    return root;
  }

  /**
   * Build a sub-tree representing the current TabStrip tree.
   */
  private static buildTabStripTree(tree: Container['tabstripTree']): ModelNode {
    const node: ModelNode = {
      path: 'Container.tabstripTree',
      value: tree,
      children: [],
      label: 'TabStrip Tree',
      displayName: 'TabStrip Tree',
    };

    tree.windows.forEach((window: WindowNode, idx: number) => {
      const winNode: ModelNode = {
        path: `${node.path}.windows[${idx}]`,
        value: window,
        children: [],
        label: `Window ${idx + 1}`,
        displayName: `Window ${idx + 1}`,
      };
      winNode.children =
          this.buildTabNodes(window.tabstripModel.root, winNode.path);
      node.children.push(winNode);
    });

    return node;
  }

  /**
   * Recursively build ModelNode objects for each tab in the TabStrip tree.
   */
  private static buildTabNodes(node: Node|null, basePath: string): ModelNode[] {
    if (!node) {
      return [];
    }

    const data = node.data;
    const out: ModelNode[] = [];

    if ('tab' in data) {
      const tab = data.tab as Tab;
      out.push({
        path: `${basePath}.tab`,
        value: tab,
        children: [],
        label: this.formatTabLabel(tab),
        displayName: tab.title,
      });
      // Recursion base case: Given node is a tab (tabs do not have children).
      // Only collections (if they have children) result in recursive calls.
      return out;
    }

    if ('tabGroupCollection' in data && data.tabGroupCollection) {
      const groupCollection = data.tabGroupCollection;
      const group: ModelNode = {
        path: `${basePath}.group`,
        value: groupCollection,
        children: [],
        label: this.formatGroupLabel(groupCollection.visualData),
        displayName: groupCollection.visualData.title,
      };
      node.children.forEach(
          (childNode: Node, childIndex: number) => {
            const tabModelNodes = this.buildTabNodes(
                childNode, `${group.path}.children[${childIndex}]`);
            for (const tab of tabModelNodes) {
              group.children.push(tab);
            }
          },
      );
      out.push(group);
      return out;
    }

    if ('splitTabCollection' in data && data.splitTabCollection) {
      const splitCollection = data.splitTabCollection;
      const split: ModelNode = {
        path: `${basePath}.split`,
        value: splitCollection,
        children: [],
        label: this.formatSplitLabel(splitCollection.visualData),
        displayName: `Split`,
      };
      node.children.forEach(
          (childNode: Node, childIndex: number) =>
              split.children.push(...this.buildTabNodes(
                  childNode, `${split.path}.children[${childIndex}]`)),
      );
      out.push(split);
      return out;
    }

    // Other container types: PinnedCollection, UnPinnedCollection, etc.
    node.children.forEach(
        (childNode: Node, childIndex: number) => out.push(...this.buildTabNodes(
            childNode, `${basePath}.children[${childIndex}]`)),
    );
    return out;
  }

  // TODO(crbug.com/427204855): Add support for TabRestore and SessionRestore
  // tabs.
  private static formatTabLabel(tab: Partial<Tab>): string {
    return `Tab: ${tab.title}`;
  }

  private static formatGroupLabel(data: TabGroupVisualData): string {
    const {color, title, isCollapsed} = data;
    return `[Group] ${title} (color: ${color}, collapsed: ${isCollapsed})`;
  }

  private static formatSplitLabel(visual: SplitTabVisualData): string {
    const {layout, splitRatio} = visual;
    return `[Split] Layout: ${layout}, Ratio: ${splitRatio}`;
  }
}
