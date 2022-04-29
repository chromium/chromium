// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles logic for the ChromeVox panel that requires state from
 * the background context.
 */
import {ISearch} from './i_search.js';
import {ISearchHandler} from './i_search_handler.js';

export class PanelBackground {
  constructor() {
    /** @private {ISearch} */
    this.iSearch_;
    /** @private {ISearchHandler} */
    this.iSearchHandler_;
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
  }

  /** Destroy the ISearch object so it can be garbage collected. */
  destroyISearch() {
    this.iSearch_.handler = null;
    this.iSearch_ = null;
  }

  /** @param {!ISearchHandler} handler */
  setISearchHandler(handler) {
    this.iSearchHandler_ = handler;
    this.iSearch_.handler = handler;
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
}

/** @type {PanelBackground} */
window.panelBackground;
