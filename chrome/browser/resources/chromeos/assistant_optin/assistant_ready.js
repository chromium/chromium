// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * ready screen.
 *
 */

Polymer({
  is: 'assistant-ready',

  behaviors: [OobeDialogHostBehavior],

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_: function() {
    if (this.buttonsDisabled) {
      return;
    }
    this.buttonsDisabled = true;
    chrome.send(
        'login.AssistantOptInFlowScreen.ReadyScreen.userActed',
        ['next-pressed']);
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    this.$['next-button'].focus();
    chrome.send('login.AssistantOptInFlowScreen.ReadyScreen.screenShown');
  },
});
