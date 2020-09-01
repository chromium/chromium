// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-receive-dialog' shows two pages:
 * 1) high visibility receive page
 * 2) Non-contact confirm page (contacts are confirmed w/ a notification)
 */

Polymer({
  is: 'nearby-share-receive-dialog',

  /**
   * @return {!CrViewManagerElement} the view manager
   * @private
   */
  getViewManager_() {
    return /** @type {!CrViewManagerElement} */ (this.$.viewManager);
  },

  /** @private */
  close_() {
    // TODO(vecore): Unregister foreground receiver.
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  },
});
