// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles logic for the ChromeVox panel that requires state from
 * the background context.
 */
import {ISearch} from './i_search.js';
import {ISearchHandler} from './i_search_handler.js';

/** @implements {ISearchHandler} */
export class PanelBackground {
  constructor() {
    /** @private {ISearch} */
    this.iSearch_;
  }

  static init() {
    if (window.panelBackground) {
      throw 'Trying to create two copies of singleton PanelBackground';
    }
    window.panelBackground = new PanelBackground();
  }

  /**
   * Creates a new ISearch object, ready to search starting from the current
   * ChromeVox focus.
   */
  createNewISearch() {
    if (this.iSearch_) {
      this.iSearch_.clear();
    }
    this.iSearch_ = new ISearch(ChromeVoxState.instance.currentRange.start);
    this.iSearch_.handler = this;
  }

  /** Destroy the ISearch object so it can be garbage collected. */
  destroyISearch() {
    this.iSearch_.handler = null;
    this.iSearch_ = null;
  }

  /**
   * @param {string} searchStr
   * @param {constants.Dir} dir
   * @param {boolean=} opt_nextObject
   */
  incrementalSearch(searchStr, dir, opt_nextObject) {
    if (!this.iSearch_) {
      console.error(
          'Trying to incrementally search when no ISearch has been created');
      return;
    }

    this.iSearch_.search(searchStr, dir, opt_nextObject);
  }

  /** Sets the current ChromeVox focus to the current ISearch node. */
  setRangeToISearchNode() {
    if (!this.iSearch_) {
      console.error(
          'Setting range to ISearch node when no ISearch in progress');
      return;
    }

    const node = this.iSearch_.cursor.node;
    if (!node) {
      return;
    }
    ChromeVoxState.instance.navigateToRange(cursors.Range.fromNode(node));
  }

  /** @override */
  onSearchReachedBoundary(boundaryNode) {
    this.iSearchOutput_(boundaryNode);
    ChromeVox.earcons.playEarcon(Earcon.WRAP);
  }

  /** @override */
  onSearchResultChanged(node, start, end) {
    this.iSearchOutput_(node, start, end);
  }

  /**
   * @param {!AutomationNode} node
   * @param {number=} opt_start
   * @param {number=} opt_end
   * @private
   */
  iSearchOutput_(node, opt_start, opt_end) {
    Output.forceModeForNextSpeechUtterance(QueueMode.FLUSH);
    const o = new Output();
    if (opt_start && opt_end) {
      o.withString([
        node.name.substr(0, opt_start),
        node.name.substr(opt_start, opt_end - opt_start),
        node.name.substr(opt_end)
      ].join(', '));
      o.format('$role', node);
    } else {
      o.withRichSpeechAndBraille(
          cursors.Range.fromNode(node), null, OutputEventType.NAVIGATE);
    }
    o.go();

    ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(node));
  }
}

/** @type {PanelBackground} */
window.panelBackground;
