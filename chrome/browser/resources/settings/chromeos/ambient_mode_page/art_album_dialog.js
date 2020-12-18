// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information for art albums.
 */

Polymer({
  is: 'art-album-dialog',

  behaviors: [I18nBehavior],

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /**
   * Closes the dialog.
   * @private
   */
  onClose_() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  },
});
