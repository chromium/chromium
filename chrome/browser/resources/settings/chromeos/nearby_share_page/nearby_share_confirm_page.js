// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-share-confirm-page' component show the user the
 * details of an incoming share request and allows the user to confirm or
 * reject the request
 */
Polymer({
  is: 'nearby-share-confirm-page',

  behaviors: [I18nBehavior],

  properties: {
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

    /**
     * @type {?nearbyShare.mojom.TransferStatus}
     */
    transferStatus: {
      type: nearbyShare.mojom.TransferStatus,
      value: null,
      observer: 'onTransferStatusChanged_',
    },

    /**
     * Header text for error. Controls error display on the confirm page.
     * The error section is not displayed if this is falsey.
     * @private {?string}
     */
    errorTitle_: {
      type: String,
      value: null,
    },

    /**
     * Description text for error display on confirm page, displayed under the
     * error title.
     * @private {?string}
     */
    errorDescription_: {
      type: String,
      value: null,
    },
  },

  /**
   * Update the |errorTitle_| and the |errorDescription_| when the transfer
   * status changes.
   * @param {?nearbyShare.mojom.TransferStatus} newStatus
   */
  onTransferStatusChanged_(newStatus) {
    // TODO(cclem) review this mapping, add missing statuses, make sure the
    // strings still make sense in the context of the receive page
    switch (newStatus) {
      case nearbyShare.mojom.TransferStatus.kTimedOut:
        this.errorTitle_ = this.i18n('nearbyShareErrorTimeOut');
        this.errorDescription_ = this.i18n('nearbyShareErrorNoResponse');
        break;
      case nearbyShare.mojom.TransferStatus.kUnsupportedAttachmentType:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
        this.errorDescription_ =
            this.i18n('nearbyShareErrorUnsupportedFileType');
        break;
      case nearbyShare.mojom.TransferStatus.kMediaUnavailable:
      case nearbyShare.mojom.TransferStatus.kNotEnoughSpace:
      case nearbyShare.mojom.TransferStatus.kFailed:
      case nearbyShare.mojom.TransferStatus.kAwaitingRemoteAcceptanceFailed:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
        this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
        break;
      default:
        this.errorTitle_ = null;
        this.errorDescription = null;
    }
  },
});
