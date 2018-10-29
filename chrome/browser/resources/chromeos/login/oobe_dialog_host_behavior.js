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
  properties: {
    /**
     * True when dialog is displayed in full-screen mode.
     */
    fullScreenDialog: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /**
   * Triggers onBeforeShow for elements matched by |selector|.
   * and sets |fullScreenDialog| attribute on them.
   * @param {string=} selector CSS selector (optional).
   */
  propagateFullScreenMode: function(selector) {
    if (!selector)
      selector = 'oobe-dialog';

    var screens = Polymer.dom(this.root).querySelectorAll(selector);
    for (var i = 0; i < screens.length; ++i) {
      if (this.fullScreenDialog)
        screens[i].fullScreenDialog = true;

      screens[i].onBeforeShow();
    }
  },

  /**
   * Pass down fullScreenDialog attribute.
   */
  onBeforeShow: function() {
    if (document.documentElement.hasAttribute('full-screen-dialog'))
      this.fullScreenDialog = true;

    this.propagateFullScreenMode();
  },

  /**
   * Triggers updateLocalizedContent() for elements matched by |selector|.
   * @param {string} selector CSS selector (optional).
   */
  propagateUpdateLocalizedContent: function(selector) {
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
