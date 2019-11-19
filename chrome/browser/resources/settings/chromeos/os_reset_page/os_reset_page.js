// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-reset-page' is the OS settings page containing reset
 * settings.
 */
Polymer({
  is: 'os-settings-reset-page',


  properties: {
    /** @private */
    showPowerwashDialog_: Boolean,
  },

  /** @private */
  /**
   * @param {!Event} e
   * @private
   */
  onShowPowerwashDialog_: function(e) {
    e.preventDefault();
    this.showPowerwashDialog_ = true;
  },

  /** @private */
  onPowerwashDialogClose_: function() {
    this.showPowerwashDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$.powerwash));
  },
});
