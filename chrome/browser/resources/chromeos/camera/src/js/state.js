// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for the app state.
 */
cca.state = cca.state || {};

/**
 * @type {!Map<string, Set<!function(boolean)>>}
 * @private
 */
cca.state.observers_ = new Map();

/**
 * Adds observer function to be called on any state change.
 * @param {string} state State to be observed.
 * @param {!function(boolean)} observer Observer function called with newly
 *     changed value.
 */
cca.state.addObserver = function(state, observer) {
  let observers = cca.state.observers_.get(state);
  if (observers === undefined) {
    observers = new Set();
    cca.state.observers_.set(state, observers);
  }
  observers.add(observer);
};

/**
 * Removes observer function to be called on state change.
 * @param {string} state State to remove observer from.
 * @param {!function(boolean)} observer Observer function to be removed.
 * @return {boolean} Whether the observer is in the set and is removed
 *     successfully or not.
 */
cca.state.removeObserver = function(state, observer) {
  const observers = cca.state.observers_.get(state);
  if (observers === undefined) {
    return false;
  }
  return observers.delete(observer);
};

/**
 * Checks if the specified state exists.
 * @param {string} state State to be checked.
 * @return {boolean} Whether the state exists.
 */
cca.state.get = function(state) {
  return document.body.classList.contains(state);
};

/**
 * Sets the specified state on or off.
 * @param {string} state State to be set.
 * @param {boolean} val True to set the state on, false otherwise.
 */
cca.state.set = function(state, val) {
  const oldVal = cca.state.get(state);
  if (oldVal == val) {
    return;
  }
  document.body.classList.toggle(state, val);
  const observers = cca.state.observers_.get(state) || [];
  observers.forEach((f) => f(val));
};
