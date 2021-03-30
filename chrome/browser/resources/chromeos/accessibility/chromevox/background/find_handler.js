// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles output for Chrome's built-in find.
 */

const TreeChangeObserverFilter = chrome.automation.TreeChangeObserverFilter;

export class FindHandler {}

/**
 * Initializes this module.
 */
FindHandler.init = function() {
  chrome.automation.addTreeChangeObserver(
      TreeChangeObserverFilter.TEXT_MARKER_CHANGES, FindHandler.onTextMatch_);
};

/**
 * Uninitializes this module.
 * @private
 */
FindHandler.uninit_ = function() {
  chrome.automation.removeTreeChangeObserver(FindHandler.onTextMatch_);
};

/**
 * @param {Object} evt
 * @private
 */
FindHandler.onTextMatch_ = function(evt) {
  if (!evt.target.markers.some(function(marker) {
        return marker.flags[chrome.automation.MarkerType.TEXT_MATCH];
      })) {
    return;
  }

  // When a user types, a flurry of events gets sent from the tree updates being
  // applied. Drop all but the first. Note that when hitting enter, there's only
  // one marker changed ever sent.
  const delta = new Date() - FindHandler.lastFindMarkerReceived;
  FindHandler.lastFindMarkerReceived = new Date();
  if (delta < FindHandler.DROP_MATCH_WITHIN_TIME_MS) {
    return;
  }

  const range = cursors.Range.fromNode(evt.target);
  ChromeVoxState.instance.setCurrentRange(range);
  new Output()
      .withRichSpeechAndBraille(range, null, Output.EventType.NAVIGATE)
      .go();
};

/**
 * The amount of time where a subsequent find text marker is dropped from
 * output.
 * @const {number}
 */
FindHandler.DROP_MATCH_WITHIN_TIME_MS = 50;

/**
 * The last time a find marker was received.
 * @type {!Date}
 */
FindHandler.lastFindMarkerReceived = new Date();
