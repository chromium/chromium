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

    /** @type {?nearbyShare.mojom.ShareTarget} */
    shareTarget: {
      type: Object,
      value: null,
    },

    /** @type {?string} */
    connectionToken: {
      type: String,
      value: null,
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

  /** @private {boolean} */
  closing_: false,

  /** @private {?nearbyShare.mojom.ReceiveManagerInterface} */
  receiveManager_: null,

  /** @private {?nearbyShare.mojom.ReceiveObserverReceiver} */
  observerReceiver_: null,

  /** @override */
  attached() {
    this.closing_ = false;
    this.errorString = null;
    this.receiveManager_ = nearby_share.getReceiveManager();
    this.observerReceiver_ = nearby_share.observeReceiveManager(
        /** @type {!nearbyShare.mojom.ReceiveObserverInterface} */ (this));
    // Request to enter high visibility mode if we have been attached.
    this.receiveManager_.enterHighVisibility();
    // TODO(vecore): determine if we need to run onboarding first or not..
    this.showHighVisibilityPage();
  },

  /** @override */
  detached() {
    if (this.observerReceiver_) {
      this.observerReceiver_.$.close();
    }

    if (this.receiveManager_) {
      /** @type {nearbyShare.mojom.ReceiveManagerRemote} */
      (this.receiveManager_).$.close();
    }
  },

  /**
   * Mojo callback when high visibility changes. If high visibility is false
   * we force this dialog to close as well.
   * @param {boolean} inHighVisibility
   */
  onHighVisibilityChanged(inHighVisibility) {
    if (inHighVisibility == false) {
      // TODO(vecore): Show error state to user
      this.close_();
    }
  },

  /**
   * Mojo callback called when a shareTarget is requesting an incoming share
   * and the user must manually confirm.
   * @param {!nearbyShare.mojom.ShareTarget} shareTarget
   * @param {?string} connectionToken
   */
  onIncomingShare(shareTarget, connectionToken) {
    this.shareTarget = shareTarget;
    this.connectionToken = connectionToken;
    this.showConfirmPage();
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
    // If we are already waiting for high visibility to exit, then we don't need
    // to trigger it again.
    if (this.closing_) {
      return;
    }

    this.closing_ = true;
    this.receiveManager_.exitHighVisibility().then(() => {
      const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
      if (dialog.open) {
        dialog.close();
      }
    });
  },

  /**
   * Call to show the high visibility page
   */
  showHighVisibilityPage() {
    this.getViewManager_().switchView(Page.HIGH_VISIBILITY);
  },

  /**
   * Call to show the share target configuration page
   */
  showConfirmPage() {
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
    this.close_();
  },

  /** @private */
  onConfirm_() {
    assert(this.shareTarget);
    this.receiveManager_.accept(this.shareTarget.id).then((success) => {
      if (success) {
        this.close_();
      } else {
        // TODO(vecore): Show error state.
        this.close_();
      }
    });
  },

  /** @private */
  onReject_() {
    assert(this.shareTarget);
    this.receiveManager_.reject(this.shareTarget.id).then((success) => {
      if (success) {
        this.close_();
      } else {
        // TODO(vecore): Show error state.
        this.close_();
      }
    });
  },
});
