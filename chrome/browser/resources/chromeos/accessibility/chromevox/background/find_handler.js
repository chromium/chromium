// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles output for Chrome's built-in find.
 */
import {CursorRange} from '/common/cursors/range.js';

import {ChromeVoxRange} from './chromevox_range.js';
import {Output} from './output/output.js';
import {OutputCustomEvent} from './output/output_types.js';

const MarkerType = chrome.automation.MarkerType;
const TreeChangeObserverFilter = chrome.automation.TreeChangeObserverFilter;

/**
 * Handles navigation among the results when using the built-in find behavior
 * (i.e. Ctrl-F).
 */
export class FindHandler {
  /** @private */
  constructor() {
    /**
     * The last time a find marker was received.
     * @type {!Date}
     */
    this.lastFindMarkerReceived = new Date();

    /** @private {function(chrome.automation.TreeChange)} */
    this.treeChangeObserver_ = change => this.onTextMatch_(change);

    chrome.automation.addTreeChangeObserver(
        TreeChangeObserverFilter.TEXT_MARKER_CHANGES, this.treeChangeObserver_);
  }

  /** Initializes this module. */
  static init() {
    if (FindHandler.instance) {
      throw 'Error: Trying to create two instances of singleton FindHandler';
    }
    FindHandler.instance = new FindHandler();
  }

  /**
   * Uninitializes this module.
   * @private
   */
  uninit_() {
    chrome.automation.removeTreeChangeObserver(this.treeChangeObserver_);
  }

  /**
   * @param {Object} evt
   * @private
   */
  onTextMatch_(evt) {
    if (!evt.target.markers.some(
            marker => marker.flags[MarkerType.TEXT_MATCH])) {
      return;
    }

    // When a user types, a flurry of events gets sent from the tree updates
    // being applied. Drop all but the first. Note that when hitting enter,
    // there's only one marker changed ever sent.
    const delta = new Date() - this.lastFindMarkerReceived;
    this.lastFindMarkerReceived = new Date();
    if (delta < DROP_MATCH_WITHIN_TIME_MS) {
      return;
    }

    const range = CursorRange.fromNode(evt.target);
    ChromeVoxRange.set(range);
    new Output()
        .withRichSpeechAndBraille(range, null, OutputCustomEvent.NAVIGATE)
        .go();
  }
}

/** @type {FindHandler} */
FindHandler.instance;

// Local to module.

/**
 * The amount of time where a subsequent find text marker is dropped from
 * output.
 * @const {number}
 */
const DROP_MATCH_WITHIN_TIME_MS = 50;
