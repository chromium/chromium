// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TabStripInternalsViewModel, ViewModelObserver} from './tab_strip_internals_viewmodel.js';

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

    // Subscribe to ViewModel state changes.
    this.viewModel_.subscribe(this);
  }

  /**
   * React to ViewModel state changes.
   */
  onViewModelChanged(): void {
    if (this.viewModel_.errorMessage) {
      this.showToast_(this.viewModel_.errorMessage);
      this.viewModel_.clearError();
    }

    this.render_();
  }

  private render_() {
    if (!this.viewModel_.root) {
      return;
    }
    this.renderTreeViewPane_();
    this.renderJsonViewPane_();
  }

  private renderTreeViewPane_() {
    // TODO(crbug.com/427204855): Implement logic to render a tree-view
    // to display the hierarchy of tabs and collections.
    this.treeViewPaneEl_;
    this.dividerEl_;
  }

  private renderJsonViewPane_() {
    // TODO(crbug.com/427204855): Implement logic to render a JSON-view
    // to display the metadata of selected tab or collection.
    this.jsonPaneEl_;
  }

  private showToast_(msg: string) {
    this.toastEl_.textContent = msg;
    this.toastEl_.classList.add('show');
    setTimeout(
        () => this.toastEl_.classList.remove('show'),
        TabStripInternalsView.TOAST_DURATION_MS);
  }
}
