// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Calculates the menu items for the node menus in the ChromeVox
 * panel.
 */

export class PanelNodeMenuBackground {
  /**
   * @param {chrome.automation.AutomationNode} node ChromeVox's current
   *     position.
   * @param {AutomationPredicate.Unary} pred Filter to use on the document.
   * @param {boolean} async If true, populates the menu asynchronously by
   *     posting a task after searching each chunk of nodes.
   * @param {Function} addMenuItem
   * @param {Function} setActiveIndex
   */
  constructor(node, pred, async, addMenuItem, setActiveIndex) {
    /** @private {AutomationNode} */
    this.node_ = node;
    /** @private {AutomationPredicate.Unary} */
    this.pred_ = pred;
    /** @private {boolean} */
    this.async_ = async;
    /** @private {Function} */
    this.addMenuItem_ = addMenuItem;
    /** @private {Function} */
    this.setActiveIndexToItemsLength_ = setActiveIndex;
    /** @private {AutomationTreeWalker|undefined} */
    this.walker_;
    /** @private {number} */
    this.nodeCount_ = 0;
    /** @private {boolean} */
    this.isEmpty_ = true;
  }

  /**
   * Create the AutomationTreeWalker and kick off the search to find
   * nodes that match the predicate for this menu.
   */
  populate() {
    if (!this.node_) {
      this.finish_();
      return;
    }

    const root = AutomationUtil.getTopLevelRoot(this.node_);
    if (!root) {
      this.finish_();
      return;
    }

    this.walker_ = new AutomationTreeWalker(root, constants.Dir.FORWARD, {
      visit(node) {
        return !AutomationPredicate.shouldIgnoreNode(node);
      }
    });
    this.nodeCount_ = 0;
    this.findMoreNodes_();
  }

  /**
   * Iterate over nodes from the tree walker. If a node matches the
   * predicate, add an item to the menu.
   *
   * If |this.async_| is true, then after MAX_NODES_BEFORE_ASYNC nodes
   * have been scanned, call setTimeout to defer searching. This frees
   * up the main event loop to keep the panel menu responsive, otherwise
   * it basically freezes up until all of the nodes have been found.
   * @private
   */
  findMoreNodes_() {
    while (this.walker_.next().node) {
      const node = this.walker_.node;
      if (this.pred_(node)) {
        this.isEmpty_ = false;
        const output = new Output();
        const range = cursors.Range.fromNode(node);
        output.withoutHints();
        output.withSpeech(range, range, OutputEventType.NAVIGATE);
        const label = output.toString();
        this.addMenuItem_(
            label, '', '', '',
            () => chrome.extension.getBackgroundPage()
                      .ChromeVoxState.instance.navigateToRange(
                          cursors.Range.fromNode(node)));

        if (node === this.node_ && !this.async_) {
          this.setActiveIndexToItemsLength_();
        }
      }

      if (this.async_) {
        this.nodeCount_++;
        if (this.nodeCount_ >= PanelNodeMenuBackground.MAX_NODES_BEFORE_ASYNC) {
          this.nodeCount_ = 0;
          window.setTimeout(this.findMoreNodes_.bind(this), 0);
          return;
        }
      }
    }
    this.finish_();
  }

  /**
   * Called when we've finished searching for nodes. If no matches were
   * found, adds an item to the menu indicating none were found.
   * @private
   */
  finish_() {
    if (this.isEmpty) {
      this.addMenuItem_(
          Msgs.getMsg('panel_menu_item_none'), '', '', '', function() {});
    }
  }
}

/**
 * The number of nodes to search before posting a task to finish
 * searching.
 * @const {number}
 */
PanelNodeMenuBackground.MAX_NODES_BEFORE_ASYNC = 100;
