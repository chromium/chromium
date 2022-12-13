// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior to be used by Polymer elements that want to
 * automatically remove WebUI listeners when detached.
 * NOTE: This file is deprecated in favor of web_ui_listener_mixin.ts. Don't use
 * it in new code.
 */

import {addWebUIListener, removeWebUIListener, WebUIListener} from 'chrome://resources/ash/common/cr.m.js';


/** @polymerBehavior */
// eslint-disable-next-line no-var
export var WebUIListenerBehavior = {
  properties: {
    /**
     * Holds WebUI listeners that need to be removed when this element is
     * destroyed.
     * @private {!Array<!WebUIListener>}
     */
    webUIListeners_: {
      type: Array,
      value() {
        return [];
      },
    },
  },

  /**
   * Adds a WebUI listener and registers it for automatic removal when this
   * element is detached.
   * Note: Do not use this method if you intend to remove this listener
   * manually (use addWebUIListener directly instead).
   *
   * @param {string} eventName The event to listen to.
   * @param {!Function} callback The callback run when the event is fired.
   */
  addWebUIListener(eventName, callback) {
    this.webUIListeners_.push(addWebUIListener(eventName, callback));
  },

  /** @override */
  detached() {
    while (this.webUIListeners_.length > 0) {
      removeWebUIListener(this.webUIListeners_.pop());
    }
  },
};

/** @interface */
export class WebUIListenerBehaviorInterface {
  /**
   * @param {string} eventName
   * @param {!Function} callback
   */
  addWebUIListener(eventName, callback) {}
}
