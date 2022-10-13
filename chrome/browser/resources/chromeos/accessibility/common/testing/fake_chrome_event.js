// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementations of ChromeEvent.
 */

class FakeChromeEvent {
  constructor() {
    /** @type {!Set<!Function>} */
    this.listeners_ = new Set();
  }

  /** @param {!Function} listener */
  addListener(listener) {
    assertFalse(
        this.listeners_.has(listener),
        'FakeChromeEvent.addListened: Listener already added');
    this.listeners_.add(listener);
  }

  /** @param {!Function} listener */
  removeListener(listener) {
    assertTrue(
        this.listeners_.has(listener),
        'FakeChromeEvent.removeListener: Listener does not exist');
    this.listeners_.delete(listener);
  }

  /** @param {...} args */
  callListeners(...args) {
    this.listeners_.forEach(function(l) {
      l(...args);
    });
  }
}
