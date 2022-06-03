// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles output for Chrome's built-in find.
 */
import {ChromeVoxState} from '/chromevox/background/chromevox_state.js';
import {Output} from '/chromevox/background/output/output.js';

const TreeChangeObserverFilter = chrome.automation.TreeChangeObserverFilter;

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
    if (!evt.target.markers.some(function(marker) {
          return marker.flags[chrome.automation.MarkerType.TEXT_MATCH];
        })) {
      return;
    }

    // When a user types, a flurry of events gets sent from the tree updates
    // being applied. Drop all but the first. Note that when hitting enter,
    // there's only one marker changed ever sent.
    const delta = new Date() - this.lastFindMarkerReceived;
    this.lastFindMarkerReceived = new Date();
    if (delta < FindHandler.DROP_MATCH_WITHIN_TIME_MS) {
      return;
    }

    const range = cursors.Range.fromNode(evt.target);
    ChromeVoxState.instance.setCurrentRange(range);
    new Output()
        .withRichSpeechAndBraille(range, null, OutputEventType.NAVIGATE)
        .go();
  }
}

/**
 * The amount of time where a subsequent find text marker is dropped from
 * output.
 * @const {number}
 */
FindHandler.DROP_MATCH_WITHIN_TIME_MS = 50;

/** @type {FindHandler} */
FindHandler.instance;
