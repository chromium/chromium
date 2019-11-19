// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'error-dialog' is a dialog that displays error messages
 * in the user manager.
 */
(function() {
Polymer({
  is: 'error-dialog',

  properties: {
    /**
     * The message shown in the dialog.
     * @private {string}
     */
    message_: {type: String, value: ''}
  },

  /**
   * Displays the dialog populated with the given message.
   * @param {string} message Error message to show.
   */
  show: function(message) {
    this.message_ = message;
    this.$.dialog.showModal();
  }
});
})();
