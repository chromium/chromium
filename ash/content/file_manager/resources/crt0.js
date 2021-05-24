// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runtime setup before main.js is executed.

/**
 * @const {boolean}
 */
window.isSWA = true;

/**
 * Listener service to local chrome.*{add,remove}Listener clients.
 */
// eslint-disable-next-line
var clientListener = clientListener || {};

clientListener.Event = class {
  constructor() {
    this.listeners_ = [];
  }

  /** @param {function()} callback */
  addListener(callback) {
    this.listeners_.push(callback);
  }

  /** @param {function()} callback */
  removeListener(callback) {
    this.listeners_ = this.listeners_.filter(l => l !== callback);
  }

  /** @param {...*} args */
  dispatchEvent(...args) {
    setTimeout(() => {
      for (const listener of this.listeners_) {
        listener(...args);
      }
    }, 0);
  }
};

// Provides dummy implementation of chrome.notification, not available in SWA.
window.chrome.notifications = {
  onClicked: new clientListener.Event(),

  onButtonClicked: new clientListener.Event(),

  onClosed: new clientListener.Event(),
};

// Provides dummy implementation of chrome.contextMenus, not available in SWA.
window.chrome.contextMenus = {
  create() {},

  remove() {},

  onClicked: new clientListener.Event(),
};
