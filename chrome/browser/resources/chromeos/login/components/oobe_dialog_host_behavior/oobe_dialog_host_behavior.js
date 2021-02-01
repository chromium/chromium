// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'OobeDialogHostBehavior' is a behavior for oobe-dialog containers to
 * match oobe-dialog bahavior.
 */

/** @polymerBehavior */
var OobeDialogHostBehavior = {
  properties: {},

  /**
   * Triggers onBeforeShow for elements matched by |selector|.
   * @param {string=} selector CSS selector (optional).
   */
  propagateOnBeforeShow(selector) {
    if (!selector)
      selector = 'oobe-dialog,oobe-adaptive-dialog';

    var screens = Polymer.dom(this.root).querySelectorAll(selector);
    for (var i = 0; i < screens.length; ++i) {
      screens[i].onBeforeShow();
    }
  },

  /**
   * Trigger onBeforeShow for all children.
   */
  onBeforeShow() {
    this.propagateOnBeforeShow();
  },

  /**
   * Triggers updateLocalizedContent() for elements matched by |selector|.
   * @param {string} selector CSS selector (optional).
   */
  propagateUpdateLocalizedContent(selector) {
    var screens = Polymer.dom(this.root).querySelectorAll(selector);
    for (var i = 0; i < screens.length; ++i) {
      /** @type {{updateLocalizedContent: function()}}}*/ (screens[i])
          .updateLocalizedContent();
    }
  },

};

/**
 * TODO(alemate): Replace with an interface. b/24294625
 * @typedef {{
 *   onBeforeShow: function()
 * }}
 */
OobeDialogHostBehavior.Proto;
