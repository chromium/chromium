// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TokenMojoType} from '//resources/mojo/mojo/public/mojom/base/token.mojom-webui.js';

import type {Container, Node, SessionSplitTab, SessionTab, SessionTabGroup, SessionWindow, SplitTabVisualData, Tab, TabGroupVisualData, TabRestoreEntry, TabRestoreGroup, TabRestoreTab, TabRestoreWindow, WindowNode} from './tab_strip_internals.mojom-webui.js';

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

    const tabstripNode = this.buildTabStripTree(container.tabstripTree);
    const tabRestoreNode = this.buildTabRestore(container.tabRestore);
    const restoredSessionNode =
        this.buildSessionRestore(container.restoredSession, 'Restored Session');
    const savedSessionNode =
        this.buildSessionRestore(container.savedSession, 'Saved Session');

    root.children.push(
        tabstripNode, tabRestoreNode, restoredSessionNode, savedSessionNode);
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

  /**
   * Build a sub-tree representing the recently closed tabs (TabRestore data).
   */
  private static buildTabRestore(restore: Container['tabRestore']): ModelNode {
    const node: ModelNode = {
      path: 'Container.tabRestore',
      value: restore,
      children: [],
      label: 'Recently Closed',
      displayName: 'Recently Closed',
    };

    restore.entries.forEach((entry: TabRestoreEntry, i: number) => {
      if ('tab' in entry) {
        const tab = entry.tab as TabRestoreTab;
        node.children.push({
          path: `${node.path}.entries[${i}]`,
          value: tab,
          children: [],
          label: this.formatTabLabel(tab),
          displayName: tab.title,
        });
      } else if ('window' in entry) {
        const window = entry.window as TabRestoreWindow;
        const windowNode: ModelNode = {
          path: `${node.path}.entries[${i}]`,
          value: window,
          children: [],
          label: `Window ${window.id.nodeId}`,
          displayName: `Window ${window.id.nodeId}`,
        };

        // Group tabs by a stable string key derived from TokenMojoType.
        const groupedTabs = new Map<string, TabRestoreTab[]>();
        const groupVisuals = new Map<string, TabGroupVisualData>();
        const ungroupedTabs: TabRestoreTab[] = [];

        for (const tab of window.tabs) {
          if (tab.groupId) {
            const groupKey = this.tokenToString_(tab.groupId);
            if (!groupedTabs.has(groupKey)) {
              groupedTabs.set(groupKey, []);
            }
            groupedTabs.get(groupKey)!.push(tab);
            if (tab.groupVisualData) {
              groupVisuals.set(groupKey, tab.groupVisualData);
            }
          } else {
            ungroupedTabs.push(tab);
          }
        }

        for (const [groupKey, groupTabs] of groupedTabs.entries()) {
          const visual = groupVisuals.get(groupKey) as TabGroupVisualData;
          const groupNode: ModelNode = {
            path: `${windowNode.path}.groups[${groupKey}]`,
            value: visual,
            children: [],
            label: this.formatGroupLabel(visual),
            displayName: visual.title,
          };
          groupTabs.forEach(
              (tab: TabRestoreTab, index: number) => groupNode.children.push({
                path: `${groupNode.path}.tabs[${index}]`,
                value: tab,
                children: [],
                label: this.formatTabLabel(tab),
                displayName: tab.title,
              }),
          );
          windowNode.children.push(groupNode);
        }
        ungroupedTabs.forEach(
            (tab: TabRestoreTab, index: number) => windowNode.children.push({
              path: `${windowNode.path}.tabs[${index}]`,
              value: tab,
              children: [],
              label: this.formatTabLabel(tab),
              displayName: tab.title,
            }),
        );
        node.children.push(windowNode);
      } else if ('group' in entry) {
        const group = entry.group as TabRestoreGroup;
        const groupNode: ModelNode = {
          path: `${node.path}.entries[${i}]`,
          value: group,
          children: [],
          label: this.formatGroupLabel(group.visualData),
          displayName: group.visualData.title,
        };
        group.tabs.forEach((tab: TabRestoreTab, index: number) => {
          groupNode.children.push({
            path: `${groupNode.path}.tabs[${index}]`,
            value: tab,
            children: [],
            label: this.formatTabLabel(tab),
            displayName: tab.title,
          });
        });
        node.children.push(groupNode);
      }
    });
    return node;
  }

  /**
   * Build a sub-tree representing SessionRestore data (either saved or
   * restored session).
   */
  private static buildSessionRestore(
      session: Container['restoredSession']|Container['savedSession'],
      label: string): ModelNode {
    // Regex removes whitespaces from label to build path string.
    const labelToPathStr = label.replace(/\s+/g, '').toLowerCase();
    const node: ModelNode = {
      path: `Container.${labelToPathStr}`,
      value: session,
      children: [],
      label: label,
      displayName: label,
    };

    if (!session || !session.entries?.length) {
      return node;
    }

    session.entries.forEach((window: SessionWindow, windowIndex: number) => {
      const windowNode: ModelNode = {
        path: `${node.path}.entries[${windowIndex}]`,
        value: window,
        children: [],
        label: `Window ${window.windowId.id}`,
        displayName: `Window ${window.windowId.id}`,
      };

      const tabGroups: SessionTabGroup[] = window.tabGroups ?? [];
      const splitTabs: SessionSplitTab[] = window.splitTabs ?? [];
      const tabs: SessionTab[] = window.tabs ?? [];

      // TokenMojoType is converted to a string representation so it can be
      // safely used as the map key.
      const groupVisualsMap = new Map<string, TabGroupVisualData>();
      for (const group of tabGroups) {
        groupVisualsMap.set(
            this.tokenToString_(group.groupId), group.visualData);
      }
      const splitVisualsMap = new Map<string, SplitTabVisualData>();
      for (const split of splitTabs) {
        splitVisualsMap.set(
            this.tokenToString_(split.splitId), split.splitVisualData);
      }

      // Building group as you go along.
      const groupNodes = new Map<string, ModelNode>();
      const splitNodes = new Map<string, ModelNode>();

      for (const tab of tabs) {
        // Convert TokenMojoType to a string representation to use it as the
        // map key.
        const groupKey = tab.groupId ? this.tokenToString_(tab.groupId) : null;
        const splitKey = tab.splitId ? this.tokenToString_(tab.splitId) : null;

        // Tab is a Grouped tab.
        if (groupKey && groupVisualsMap.has(groupKey)) {
          let groupNode = groupNodes.get(groupKey);
          if (!groupNode) {
            const visualData =
                groupVisualsMap.get(groupKey) as TabGroupVisualData;
            groupNode = {
              path: `${windowNode.path}.groups[${groupKey}]`,
              value: visualData,
              children: [],
              label: this.formatGroupLabel(visualData),
              displayName: visualData.title,
            };
            groupNodes.set(groupKey, groupNode);
            windowNode.children.push(groupNode);
          }

          groupNode.children.push({
            path: `${groupNode.path}.tabs[${groupNode.children.length}]`,
            value: tab,
            children: [],
            label: this.formatTabLabel(tab),
            displayName: tab.title,
          });
          continue;
        }

        // Tab is a Split tab.
        if (splitKey && splitVisualsMap.has(splitKey)) {
          let splitNode = splitNodes.get(splitKey);
          if (!splitNode) {
            const visualData =
                splitVisualsMap.get(splitKey) as SplitTabVisualData;
            splitNode = {
              path: `${windowNode.path}.splits[${splitKey}]`,
              value: visualData,
              children: [],
              label: this.formatSplitLabel(visualData),
              displayName: 'Split',
            };
            splitNodes.set(splitKey, splitNode);
            windowNode.children.push(splitNode);
          }

          splitNode.children.push({
            path: `${splitNode.path}.tabs[${splitNode.children.length}]`,
            value: tab,
            children: [],
            label: this.formatTabLabel(tab),
            displayName: tab.title,
          });
          continue;
        }

        // Tab is a regular tab (unpinned or pinned).
        windowNode.children.push({
          path: `${windowNode.path}.tabs[${windowNode.children.length}]`,
          value: tab,
          children: [],
          label: this.formatTabLabel(tab),
          displayName: tab.title,
        });
      }

      node.children.push(windowNode);
    });

    return node;
  }

  private static formatTabLabel(tab: Partial<Tab|TabRestoreTab|SessionTab>):
      string {
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

  /**
   * Convert a Mojo Token to its canonical string representation i.e. represents
   * a 128-bit value as two 64-bit words using zero-padded uppercase
   * hexadecimal format.
   *
   * Note: This mirrors C++ representation of base::Token::ToString().
   */
  private static tokenToString_(token: TokenMojoType): string {
    const hi = token.high.toString(16).padStart(16, '0').toUpperCase();
    const lo = token.low.toString(16).padStart(16, '0').toUpperCase();
    return `${hi}${lo}`;
  }
}
