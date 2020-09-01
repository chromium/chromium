// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-receive-dialog' shows two pages:
 * 1) high visibility receive page
 * 2) Non-contact confirm page (contacts are confirmed w/ a notification)
 */

/** @enum {string} */
const Page = {
  HIGH_VISIBILITY: 'high-visibility',
  CONFIRM: 'confirm',
};

Polymer({
  is: 'nearby-share-receive-dialog',

  properties: {
    /** Mirroring the enum so that it can be used from HTML bindings. */
    Page: {
      type: Object,
      value: Page,
    },

    /** @type {nearby_share.NearbySettings} */
    settings: {
      type: Object,
      value: {},
    },
  },

  listeners: {
    'change-page': 'onChangePage_',
    'cancel': 'onCancel_',
    'confirm': 'onConfirm_',
    'reject': 'onReject_',
  },

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

  /**
   * Call to show the high visibility page
   */
  showHighVisibilityPage() {
    // TODO(vecore): Register foreground receiver.
    this.getViewManager_().switchView(Page.HIGH_VISIBILITY);
  },

  /**
   * Call to show the share target configuration page
   */
  showConfirmPage() {
    // TODO (vecore): Extract share target state or pass in.
    this.getViewManager_().switchView(Page.CONFIRM);
  },

  /**
   * Child views can fire a 'change-page' event to trigger a page change
   * @param {!CustomEvent<!{page: Page}>} event
   * @private
   */
  onChangePage_(event) {
    this.getViewManager_().switchView(event.detail.page);
  },

  /** @private */
  onCancel_() {
    // TODO(vecore): exit high-visibility mode
    this.close_();
  },

  /** @private */
  onConfirm_() {
    // TODO(vecore): accept share target
    this.close_();
  },

  /** @private */
  onReject_() {
    // TODO(vecore): reject share target
    this.close_();
  },
});
