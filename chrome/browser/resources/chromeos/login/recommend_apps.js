// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Recommend Apps
 * screen.
 */

Polymer({
  is: 'recommend-apps',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Reference to OOBE screen object.
     * @type {!{
     *     onInstall: function(),
     *     onRetry: function(),
     *     onSkip: function(),
     * }}
     */
    screen: {
      type: Object,
    },
  },

  focus: function() {
    this.getElement('recommend-apps-dialog').focus();
  },

  /** @private */
  onSkip_: function() {
    this.screen.onSkip();
  },

  /** @private */
  onInstall_: function() {
    this.screen.onInstall();
  },

  /** @private */
  onRetry_: function() {
    this.screen.onRetry();
  },

  /**
   * Returns element by its id.
   * @param id String The ID of the element.
   */
  getElement: function(id) {
    return this.$[id];
  },
});
