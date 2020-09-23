// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-confirmation-page' component shows a confirmation
 * screen when sending data to a stranger. Strangers are devices of people that
 * are not currently in the contacts of this user.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './nearby_preview.js';
import './nearby_progress.js';
import './nearby_share_target_types.mojom-lite.js';
import './nearby_share.mojom-lite.js';
import './shared/nearby_page_template.m.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @implements {nearbyShare.mojom.TransferUpdateListenerInterface} */
class TransferUpdateListener {
  /**
   * @param {!NearbyConfirmationPageElement} page
   * @param {!nearbyShare.mojom.TransferUpdateListenerPendingReceiver}
   *     transferUpdateListener
   */
  constructor(page, transferUpdateListener) {
    this.page_ = page;
    this.transferUpdateListenerReceiver_ =
        new nearbyShare.mojom.TransferUpdateListenerReceiver(this);
    this.transferUpdateListenerReceiver_.$.bindHandle(
        transferUpdateListener.handle);
  }

  /**
   * @param {!nearbyShare.mojom.TransferStatus} status The status update.
   * @param {?string} token The optional token to show to the user.
   * @private
   * @override
   */
  onTransferUpdate(status, token) {
    this.page_.onTransferUpdate(status, token);
  }
}

Polymer({
  is: 'nearby-confirmation-page',

  behaviors: [I18nBehavior],

  _template: html`{__html_template__}`,

  properties: {
    /**
     * ConfirmationManager interface for the currently selected share target.
     * Expected to start as null, then change to a valid object before this
     * component is shown.
     * @type {?nearbyShare.mojom.ConfirmationManagerInterface}
     */
    confirmationManager: {
      type: Object,
      value: null,
    },

    /**
     * TransferUpdateListener interface for the currently selected share target.
     * Expected to start as null, then change to a valid object before this
     * component is shown.
     * @type {?nearbyShare.mojom.TransferUpdateListenerPendingReceiver}
     */
    transferUpdateListener: {
      type: Object,
      value: null,
      observer: 'onTransferUpdateListenerChanged_'
    },

    /**
     * The selected share target to confirm the transfer for. Expected to start
     * as null, then change to a valid object before this component is shown.
     * @type {?nearbyShare.mojom.ShareTarget}
     */
    shareTarget: {
      type: Object,
      value: null,
    },

    /**
     * Token to show to the user to confirm the selected share target. Expected
     * to start as null, then change to a valid object via updates from the
     * transferUpdateListener.
     * @private {?string}
     */
    confirmationToken_: {
      type: String,
      value: null,
    },

    /**
     * Whether the user needs to confirm this transfer on the local device. This
     * affects which buttons are displayed to the user.
     * @private
     * */
    needsConfirmation_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'accept': 'onAccept_',
    'reject': 'onReject_',
    'cancel': 'onCancel_',
  },

  /** @private {?TransferUpdateListener} */
  transferUpdateListener_: null,

  /**
   * @param {?nearbyShare.mojom.TransferUpdateListenerPendingReceiver}
   *     transferUpdateListener
   * @private
   */
  onTransferUpdateListenerChanged_(transferUpdateListener) {
    if (!transferUpdateListener) {
      return;
    }

    this.transferUpdateListener_ =
        new TransferUpdateListener(this, transferUpdateListener);
  },

  /**
   * @param {!nearbyShare.mojom.TransferStatus} status The status update.
   * @param {?string} token The optional token to show to the user.
   */
  onTransferUpdate(status, token) {
    if (token) {
      this.confirmationToken_ = token;
    }

    switch (status) {
      case nearbyShare.mojom.TransferStatus.kAwaitingLocalConfirmation:
        this.needsConfirmation_ = true;
        break;
      case nearbyShare.mojom.TransferStatus.kAwaitingRemoteAcceptance:
        this.needsConfirmation_ = false;
        break;
      case nearbyShare.mojom.TransferStatus.kInProgress:
        this.fire('close');
        break;
    }
  },

  /**
   * @return {string} The contact name of the selected ShareTarget.
   * @private
   */
  contactName_() {
    // TODO(crbug.com/1123943): Get contact name from ShareTarget.
    const contactName = null;
    if (!contactName) {
      return '';
    }
    return this.i18n('nearbyShareConfirmationPageAddContactTitle', contactName);
  },

  /** @private */
  onAccept_() {
    this.confirmationManager.accept().then(
        result => {
            // TODO(crbug.com/1123934): Show error if !result.success
        });
  },

  /** @private */
  onReject_() {
    this.confirmationManager.reject().then(result => {
      this.fire('close');
    });
  },

  /** @private */
  onCancel_() {
    this.confirmationManager.cancel().then(result => {
      this.fire('close');
    });
  },

  /**
   * @param {boolean} needsConfirmation
   * @return {?string} Localized string or null if the button should be hidden.
   */
  getActionButtonLabel_(needsConfirmation) {
    return needsConfirmation ? this.i18n('nearbyShareActionsConfirm') : null;
  },

  /**
   * @param {boolean} needsConfirmation
   * @return {string} Localized string to show on the cancel button.
   * @private
   */
  getCancelButtonLabel_(needsConfirmation) {
    return needsConfirmation ? this.i18n('nearbyShareActionsReject') :
                               this.i18n('nearbyShareActionsCancel');
  },

  /**
   * @param {boolean} needsConfirmation
   * @return {string} The event name fire when the cancel button is clicked.
   * @private
   */
  getCancelEventName_(needsConfirmation) {
    return needsConfirmation ? 'reject' : 'cancel';
  },

  /**
   * @return {!string} The title of the attachment to be shared.
   * @private
   */
  attachmentTitle_() {
    // TODO(crbug.com/1123942): Pass attachments to UI.
    return 'Unknown file';
  },
});
