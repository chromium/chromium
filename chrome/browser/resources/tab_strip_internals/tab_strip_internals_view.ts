// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ModelNode} from './tab_strip_internals_adapter.js';
import type {TabStripInternalsViewModel, ViewModelObserver} from './tab_strip_internals_viewmodel.js';
import {ViewModelChange} from './tab_strip_internals_viewmodel.js';

/**
 * View layer: Handles presentation and user interaction for the TabStrip
 * Internals page.
 */
export class TabStripInternalsView implements ViewModelObserver {
  private viewModel_: TabStripInternalsViewModel;
  /**
   * Represents the left pane element used to display a tree-view of nodes
   * (a hierarchy of tabs and tabcollections).
   */
  private treeViewPaneEl_: HTMLElement;
  /**
   * Represents the right pane element used to display a JSON-view of node
   * metadata.
   */
  private jsonPaneEl_: HTMLElement;
  /** Represents the divider element between left and right panes. */
  private dividerEl_: HTMLElement;
  /** Represents a notification toast element. */
  private toastEl_: HTMLElement;
  private static readonly TOAST_DURATION_MS: number = 1500;

  constructor(viewModel: TabStripInternalsViewModel) {
    this.viewModel_ = viewModel;
    this.treeViewPaneEl_ = document.getElementById('treeViewPane')!;
    this.jsonPaneEl_ = document.getElementById('jsonPane')!;
    this.dividerEl_ = document.getElementById('divider')!;
    this.toastEl_ = document.getElementById('toast')!;

    // Setup event listeners.
    this.setupDividerListeners_();

    // Subscribe to ViewModel state changes.
    this.viewModel_.subscribe(this);
  }

  /**
   * React to ViewModel state changes.
   */
  onViewModelChanged(change: ViewModelChange): void {
    switch (change) {
      case ViewModelChange.NOTIFICATION:
        this.showToast_(this.viewModel_.errorMessage!);
        this.viewModel_.clearError();
        return;

      case ViewModelChange.LAYOUT:
        this.updateSplitLayout_();
        return;

      case ViewModelChange.CONTENT:
        this.render_();
        return;

      default:
        console.warn('Unhandled ViewModelChange:', change);
        return;
    }
  }

  /** Root render method used to render the entire view. */
  private render_() {
    if (!this.viewModel_.root) {
      return;
    }
    this.renderTreeViewPane_();
    this.renderJsonViewPane_();
  }

  /**
   * Render treeView pane to display a hierarchy of tabs and collections in
   * the navigation panel present on the left side.
   */
  private renderTreeViewPane_() {
    this.treeViewPaneEl_.replaceChildren(
        this.renderTreeNode_(this.viewModel_.root));
  }

  /** Recursively render nodes for the tree view. */
  private renderTreeNode_(node: ModelNode): HTMLElement {
    const nodeLabel = node.label;
    const nodePath = node.path;
    const isSelected = this.viewModel_.selectedNode === nodePath;
    const isExpanded = this.viewModel_.isExpanded(nodePath);
    const canExpand = this.viewModel_.hasChildren(nodePath);

    const wrapEl = document.createElement('div');
    wrapEl.className = 'tree-node-wrap';

    const nodeEl = document.createElement('div');
    nodeEl.className = 'tree-node' + (isSelected ? ' selected' : '');
    nodeEl.dataset['path'] = nodePath;

    const expandEl = document.createElement('span');
    expandEl.className = 'expander';
    expandEl.textContent = canExpand ? (isExpanded ? '▾' : '▸') : '';

    const nodeLabelEl = document.createElement('span');
    nodeLabelEl.textContent = nodeLabel;

    nodeEl.append(expandEl, nodeLabelEl);
    const icon = this.renderNodeIcon_(node);
    if (icon) {
      nodeEl.append(icon);
    }

    nodeEl.onclick = this.handleNodeClick_.bind(this, node);
    expandEl.onclick = this.handleExpanderClick_.bind(this, node);

    wrapEl.append(nodeEl);

    // Render children recursively if expanded.
    if (isExpanded) {
      const childrenEl = document.createElement('div');
      childrenEl.className = 'tree-children';
      for (const childNode of this.viewModel_.getChildren(nodePath)) {
        childrenEl.appendChild(this.renderTreeNode_(childNode));
      }
      wrapEl.append(childrenEl);
    }

    return wrapEl;
  }

  /**
   * Render jsonView pane to display a metadata about selected node in
   * the content panel present on the right side.
   */
  private renderJsonViewPane_() {
    // TODO(crbug.com/427204855): Implement logic to render a JSON-view
    // to display the metadata of selected tab or collection.
    this.jsonPaneEl_;
  }

  /** Display a toast message. */
  private showToast_(msg: string) {
    this.toastEl_.textContent = msg;
    this.toastEl_.classList.add('show');
    setTimeout(
        () => this.toastEl_.classList.remove('show'),
        TabStripInternalsView.TOAST_DURATION_MS);
  }

  // DOM manipulation.
  private updateSplitLayout_(): void {
    document.getElementById('split')!.style.gridTemplateColumns =
        `${this.viewModel_.navPaneWidth}px 5px 1fr`;
  }

  /** Returns a container span with all applicable icons. */
  private renderNodeIcon_(node: ModelNode): HTMLElement|null {
    const value = node.value as any;
    const iconsEl: HTMLElement[] = [];

    if (value?.pinned) {
      iconsEl.push(this.makeIcon('pinned', 'Pinned'));
    }
    if (value?.selected) {
      iconsEl.push(this.makeIcon('check', 'Selected'));
    }
    if (value?.active) {
      iconsEl.push(this.makeIcon('dot active', 'Active'));
    }
    if (!iconsEl.length) {
      return null;
    }

    const group = document.createElement('span');
    group.className = 'icon-group';
    for (const icon of iconsEl) {
      group.appendChild(icon);
    }
    return group;
  }

  /** Returns an icon element. */
  private makeIcon(iconClass: string, iconTitle: string): HTMLElement {
    const element = document.createElement('span');
    element.className = `icon ${iconClass}`;
    element.title = iconTitle;
    return element;
  }

  // Event handlers.
  private setupDividerListeners_() {
    this.dividerEl_.addEventListener(
        'mousedown', this.handleDividerMouseDown_.bind(this));
    this.dividerEl_.onkeydown = this.handleDividerKeydown_.bind(this);
  }

  /** Handles resizing of the sections via mouse events. */
  private handleDividerMouseDown_(e: MouseEvent) {
    let lastX = e.clientX;
    let rAF = 0;

    const move = (ev: MouseEvent) => {
      if (rAF) {
        return;
      }
      rAF = requestAnimationFrame(() => {
        rAF = 0;
        const delta = ev.clientX - lastX;
        lastX = ev.clientX;
        this.viewModel_.setNavPaneWidth(this.viewModel_.navPaneWidth + delta);
      });
    };

    const up = () => {
      if (rAF) {
        cancelAnimationFrame(rAF);
        rAF = 0;
      }
      window.removeEventListener('mousemove', move);
      window.removeEventListener('mouseup', up);
      this.viewModel_.saveNavPaneWidthState();
    };

    window.addEventListener('mousemove', move);
    window.addEventListener('mouseup', up);
  }

  /** Handles resizing of the sections via keyboard. */
  private handleDividerKeydown_(e: KeyboardEvent) {
    const step = e.shiftKey ? 50 : 10;
    switch (e.key) {
      case 'ArrowLeft':
        this.viewModel_.setNavPaneWidth(this.viewModel_.navPaneWidth - step);
        break;
      case 'ArrowRight':
        this.viewModel_.setNavPaneWidth(this.viewModel_.navPaneWidth + step);
        break;
      default:
        return;
    }
    this.viewModel_.saveNavPaneWidthState();
    e.preventDefault();
  }

  /** Handle clicks on a tree node within the treeView pane. */
  private handleNodeClick_(node: ModelNode) {
    const path = node.path;
    if (this.viewModel_.selectedNode !== path) {
      this.viewModel_.setSelectedNode(path);
    }

    // Clicking a node should expand or collapse the sub-tree for
    // user-convenience.
    // It mimics the same behavior that occurs when user clicks on the
    // expander of a node.
    const canExpand = this.viewModel_.hasChildren(path);
    if (canExpand) {
      this.viewModel_.toggleExpanded(path);
    }
  }

  /**
   * Handle clicks on the expander (expand-collapse icon) within a node in the
   * treeView pane.
   */
  private handleExpanderClick_(node: ModelNode, e: MouseEvent) {
    e.stopPropagation();
    const path = node.path;
    const canExpand = this.viewModel_.hasChildren(path);

    if (!canExpand) {
      return;
    }
    this.viewModel_.toggleExpanded(path);
  }
}
