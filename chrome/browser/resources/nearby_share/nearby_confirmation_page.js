// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-confirmation-page' component shows a confirmation
 * screen when sending data to a stranger. Strangers are devices of people that
 * are not currently in the contacts of this user.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './shared/nearby_page_template.js';
import './shared/nearby_preview.js';
import './shared/nearby_progress.js';
import './strings.m.js';

import {ConfirmationManagerInterface, PayloadPreview, ShareTarget, TransferStatus, TransferUpdateListenerInterface, TransferUpdateListenerPendingReceiver, TransferUpdateListenerReceiver} from '/mojo/nearby_share.mojom-webui.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getDiscoveryManager} from './discovery_manager.js';
import {getTemplate} from './nearby_confirmation_page.html.js';
import {CloseReason} from './shared/types.js';

/** @implements {TransferUpdateListenerInterface} */
class TransferUpdateListener {
  /**
   * @param {!NearbyConfirmationPageElement} page
   * @param {!TransferUpdateListenerPendingReceiver} transferUpdateListener
   */
  constructor(page, transferUpdateListener) {
    this.page_ = page;
    this.transferUpdateListenerReceiver_ =
        new TransferUpdateListenerReceiver(this);
    this.transferUpdateListenerReceiver_.$.bindHandle(
        transferUpdateListener.handle);
  }

  /**
   * @param {!TransferStatus} status The status update.
   * @param {?string} token The optional token to show to the user.
   * @private
   * @override
   */
  onTransferUpdate(status, token) {
    this.page_.onTransferUpdate(status, token);
  }
}

/**
 * The progress bar asset URL for light mode
 * @type {string}
 */
const PROGRESS_BAR_URL_LIGHT = 'nearby_share_progress_bar_light.json';

/**
 * The progress bar asset URL for dark mode
 * @type {string}
 */
const PROGRESS_BAR_URL_DARK = 'nearby_share_progress_bar_dark.json';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NearbyConfirmationPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class NearbyConfirmationPageElement extends
    NearbyConfirmationPageElementBase {
  static get is() {
    return 'nearby-confirmation-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * ConfirmationManager interface for the currently selected share target.
       * Expected to start as null, then change to a valid object before this
       * component is shown.
       * @type {?ConfirmationManagerInterface}
       */
      confirmationManager: {
        type: Object,
        value: null,
      },

      /**
       * TransferUpdateListener interface for the currently selected share
       * target. Expected to start as null, then change to a valid object before
       * this component is shown.
       * @type {?TransferUpdateListenerPendingReceiver}
       */
      transferUpdateListener: {
        type: Object,
        value: null,
        observer: 'onTransferUpdateListenerChanged_',
      },

      /**
       * The selected share target to confirm the transfer for. Expected to
       * start as null, then change to a valid object before this component is
       * shown.
       * @type {?ShareTarget}
       */
      shareTarget: {
        type: Object,
        value: null,
      },

      /**
       * Preview info for the file(s) to send. Expected to start
       * as null, then change to a valid object before this component is shown.
       * @type {?PayloadPreview}
       */
      payloadPreview: {
        type: Object,
        value: null,
      },

      /**
       * Token to show to the user to confirm the selected share target.
       * Expected to start as null, then change to a valid object via updates
       * from the transferUpdateListener.
       * @private {?string}
       */
      confirmationToken_: {
        type: String,
        value: null,
      },

      /**
       * Header text for error. The error section is not displayed if this is
       * falsey.
       * @private {?string}
       */
      errorTitle_: {
        type: String,
        value: null,
      },

      /**
       * Description text for error, displayed under the error title.
       * @private {?string}
       */
      errorDescription_: {
        type: String,
        value: null,
      },

      /**
       * Whether the user needs to confirm this transfer on the local device.
       * This affects which buttons are displayed to the user.
       * @private
       * */
      needsConfirmation_: {
        type: Boolean,
        value: false,
      },

      /**
       * @private {?TransferStatus}
       */
      lastTransferStatus_: {
        type: TransferStatus,
        value: null,
      },

      /**
       * Whether the confirmation page is being rendered in dark mode.
       * @private {boolean}
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

    };
  }

  constructor() {
    super();

    /** @private {?TransferUpdateListener} */
    this.transferUpdateListener_ = null;
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener('accept', this.onAccept_);
    this.addEventListener('reject', this.onReject_);
    this.addEventListener('cancel', this.onCancel_);
  }

  /**
   * @param {string} eventName
   * @param {*=} detail
   * @private
   */
  fire_(eventName, detail) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  /**
   * @return {!Object} The transferStatus, errorTitle, and errorDescription.
   * @public
   */
  getTransferInfoForTesting() {
    return {
      confirmationToken: this.confirmationToken_,
      transferStatus: this.lastTransferStatus_,
      errorTitle: this.errorTitle_,
      errorDescription: this.errorDescription_,
    };
  }

  /**
   * @param {?TransferUpdateListenerPendingReceiver} transferUpdateListener
   * @private
   */
  onTransferUpdateListenerChanged_(transferUpdateListener) {
    if (!transferUpdateListener) {
      return;
    }

    this.transferUpdateListener_ =
        new TransferUpdateListener(this, transferUpdateListener);
  }

  /**
   * @param {!TransferStatus} status The status update.
   * @param {?string} token The optional token to show to the user.
   */
  onTransferUpdate(status, token) {
    if (token) {
      this.confirmationToken_ = token;
    }
    this.lastTransferStatus_ = status;

    switch (status) {
      case TransferStatus.kAwaitingLocalConfirmation:
        this.needsConfirmation_ = true;
        break;
      case TransferStatus.kAwaitingRemoteAcceptance:
        this.needsConfirmation_ = false;
        break;
      case TransferStatus.kInProgress:
        getDiscoveryManager().stopDiscovery().then(
            () => this.fire_('close', {reason: CloseReason.TRANSFER_STARTED}));
        break;
      case TransferStatus.kComplete:
        getDiscoveryManager().stopDiscovery().then(
            () =>
                this.fire_('close', {reason: CloseReason.TRANSFER_SUCCEEDED}));
        break;
      case TransferStatus.kRejected:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
        this.errorDescription_ = this.i18n('nearbyShareErrorRejected');
        break;
      case TransferStatus.kTimedOut:
        this.errorTitle_ = this.i18n('nearbyShareErrorTimeOut');
        this.errorDescription_ = this.i18n('nearbyShareErrorNoResponse');
        break;
      case TransferStatus.kUnsupportedAttachmentType:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
        this.errorDescription_ =
            this.i18n('nearbyShareErrorUnsupportedFileType');
        break;
      case TransferStatus.kMediaUnavailable:
      case TransferStatus.kNotEnoughSpace:
      case TransferStatus.kFailed:
      case TransferStatus.kAwaitingRemoteAcceptanceFailed:
      case TransferStatus.kDecodeAdvertisementFailed:
      case TransferStatus.kMissingTransferUpdateCallback:
      case TransferStatus.kMissingShareTarget:
      case TransferStatus.kMissingEndpointId:
      case TransferStatus.kMissingPayloads:
      case TransferStatus.kPairedKeyVerificationFailed:
      case TransferStatus.kInvalidIntroductionFrame:
      case TransferStatus.kIncompletePayloads:
      case TransferStatus.kFailedToCreateShareTarget:
      case TransferStatus.kFailedToInitiateOutgoingConnection:
      case TransferStatus.kFailedToReadOutgoingConnectionResponse:
      case TransferStatus.kUnexpectedDisconnection:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantShare');
        this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
        break;
    }
  }

  /**
   * @return {string} The contact name of the selected ShareTarget.
   * @private
   */
  contactName_() {
    // TODO(crbug.com/1123943): Get contact name from ShareTarget.
    const contactName = null;
    if (!contactName || this.errorTitle_) {
      return '';
    }
    return this.i18n('nearbyShareConfirmationPageAddContactTitle', contactName);
  }

  /** @private */
  onAccept_() {
    this.confirmationManager.accept().then(
        result => {
            // TODO(crbug.com/1123934): Show error if !result.success
        });
  }

  /** @private */
  onReject_() {
    this.confirmationManager.reject().then(result => {
      this.fire_('close', {reason: CloseReason.REJECTED});
    });
  }

  /** @private */
  onCancel_() {
    this.confirmationManager.cancel().then(result => {
      this.fire_('close', {reason: CloseReason.CANCELLED});
    });
  }

  /**
   * @param {boolean} needsConfirmation
   * @return {?string} Localized string or null if the button should be hidden.
   */
  getActionButtonLabel_(needsConfirmation) {
    return needsConfirmation ? this.i18n('nearbyShareActionsConfirm') : null;
  }

  /**
   * @param {boolean} needsConfirmation
   * @return {string} Localized string to show on the cancel button.
   * @private
   */
  getCancelButtonLabel_(needsConfirmation) {
    return needsConfirmation ? this.i18n('nearbyShareActionsReject') :
                               this.i18n('nearbyShareActionsCancel');
  }

  /**
   * @param {boolean} needsConfirmation
   * @return {string} The event name fire when the cancel button is clicked.
   * @private
   */
  getCancelEventName_(needsConfirmation) {
    return needsConfirmation ? 'reject' : 'cancel';
  }

  /**
   * @return {!string} The title of the attachment to be shared.
   * @private
   */
  attachmentTitle_() {
    return this.payloadPreview && this.payloadPreview.description ?
        this.payloadPreview.description :
        'Unknown file';
  }

  /**
   * Returns the URL for the asset that defines a file transfer's animated
   * progress bar.
   */
  getAnimationUrl_() {
    return this.isDarkModeActive_ ? PROGRESS_BAR_URL_DARK :
                                    PROGRESS_BAR_URL_LIGHT;
  }
}

customElements.define(
    NearbyConfirmationPageElement.is, NearbyConfirmationPageElement);
