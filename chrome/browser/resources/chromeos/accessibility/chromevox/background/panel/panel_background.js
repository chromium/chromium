// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles logic for the ChromeVox panel that requires state from
 * the background context.
 */
import {ISearch} from './i_search.js';

// This class is imported for its side effects.
import {PanelNodeMenuBackground} from './panel_node_menu_background.js';

export class PanelBackground {
  /** @private */
  constructor() {
    /** @private {ISearch} */
    this.iSearch_;
  }

  static init() {
    if (PanelBackground.instance) {
      throw 'Error: PanelBackground initiated more than once';
    }
    PanelBackground.instance = new PanelBackground();
  }

  /** @private */
  createNewISearch_() {
    if (this.iSearch_) {
      this.iSearch_.clear();
    }
    this.iSearch_ = new ISearch(ChromeVoxState.instance.currentRange.start);
  }

  /**
   * @param {string} searchStr
   * @param {constants.Dir} dir
   * @param {boolean=} opt_nextObject
   * @private
   */
  incrementalSearch_(searchStr, dir, opt_nextObject) {
    if (!this.iSearch_) {
      console.error(
          'Trying to incrementally search when no ISearch has been created.');
      return;
    }

    this.iSearch_.search(searchStr, dir, opt_nextObject);
  }

  /** @private */
  setRangeToISearchNode_() {
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

BridgeHelper.registerHandler(
    /* target= */ 'PanelBackground', 'createNewISearch',
    () => PanelBackground.instance.createNewISearch_());
BridgeHelper.registerHandler(
    /* target= */ 'PanelBackground', 'incrementalSearch',
    ({searchStr, dir, opt_nextObject}) =>
        PanelBackground.instance.incrementalSearch_(
            searchStr, dir, opt_nextObject));
BridgeHelper.registerHandler(
    /* target= */ 'PanelBackground', 'setRangeToISearchNode',
    () => PanelBackground.instance.setRangeToISearchNode_());
