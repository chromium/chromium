// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Handles output for Chrome's built-in find.
 */
goog.provide('FindHandler');

goog.require('Output');

goog.scope(function() {
var TreeChangeObserverFilter = chrome.automation.TreeChangeObserverFilter;

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
  if (!evt.target.markerTypes.some(function(markerType) {
        return markerType & 4 /* Text match */;
      }))
    return;

  var range = cursors.Range.fromNode(evt.target);
  ChromeVoxState.instance.setCurrentRange(range);
  new Output()
      .withRichSpeechAndBraille(range, null, Output.EventType.NAVIGATE)
      .go();
};
});  // goog.scope
