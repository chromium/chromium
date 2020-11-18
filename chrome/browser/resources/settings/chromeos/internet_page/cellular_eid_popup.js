// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying cellular EID and QR code
 */

Polymer({
  is: 'cellular-eid-popup',

  behaviors: [
    I18nBehavior,
  ],

  /** @override */
  focus() {
    this.$$('.dialog').focus();
  },

  /**@private */
  onCloseTap_() {
    this.fire('close-eid-popup');
  },
});
