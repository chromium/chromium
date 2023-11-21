// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-share-confirm-page' component show the user the
 * details of an incoming share request and allows the user to confirm or
 * reject the request
 */

import 'chrome://resources/cr_elements/cr_lottie/cr_lottie.js';
import 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '/shared/nearby_page_template.js';
import '/shared/nearby_device.js';
import '/shared/nearby_preview.js';
import '/shared/nearby_progress.js';

import {ShareTarget, TransferStatus} from '/shared/nearby_share.mojom-webui.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_share_confirm_page.html.js';

/**
 * The progress bar asset URL for light mode.
 */
const PROGRESS_BAR_URL_LIGHT = 'nearby_share_progress_bar_light.json';

/**
 * The progress bar asset URL for dark mode.
 */
const PROGRESS_BAR_URL_DARK = 'nearby_share_progress_bar_dark.json';

/**
 * The progress bar asset URL for jelly mode.
 */
const PROGRESS_BAR_URL_JELLY = 'nearby_share_progress_bar_jelly.json';

const NearbyShareConfirmPageElementBase = I18nMixin(PolymerElement);

export class NearbyShareConfirmPageElement extends
    NearbyShareConfirmPageElementBase {
  static get is() {
    return 'nearby-share-confirm-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shareTarget: {
        type: Object,
        value: null,
      },

      connectionToken: {
        type: String,
        value: null,
      },

      transferStatus: {
        type: TransferStatus,
        value: null,
        observer: 'onTransferStatusChanged_',
      },

      /**
       * Header text for error. Controls error display on the confirm page.
       * The error section is not displayed if this is falsey.
       */
      errorTitle_: {
        type: String,
        value: null,
      },

      /**
       * Description text for error display on confirm page, displayed under the
       * error title.
       */
      errorDescription_: {
        type: String,
        value: null,
      },

      /**
       * Whether the confirm page is being rendered in dark mode.
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

      /**
       * Return true if the Jelly feature flag is enabled.
       */
      isJellyEnabled_: {
        type: Boolean,
        readOnly: true,
        value() {
          return loadTimeData.valueExists('isJellyEnabled') &&
              loadTimeData.getBoolean('isJellyEnabled');
        },
      },
    };
  }

  connectionToken: string|null;
  shareTarget: ShareTarget|null;
  transferStatus: TransferStatus|null;
  private errorDescription_: string|null;
  private errorTitle_: string|null;
  private isDarkModeActive_: boolean;
  private isJellyEnabled_: boolean;

  /**
   * Update the |errorTitle_| and the |errorDescription_| when the transfer
   * status changes.
   */
  private onTransferStatusChanged_(newStatus: TransferStatus|null): void {
    switch (newStatus) {
      case TransferStatus.kTimedOut:
        this.errorTitle_ = this.i18n('nearbyShareErrorTimeOut');
        this.errorDescription_ = this.i18n('nearbyShareErrorTryAgain');
        break;
      case TransferStatus.kUnsupportedAttachmentType:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantReceive');
        this.errorDescription_ =
            this.i18n('nearbyShareErrorUnsupportedFileType');
        break;
      case TransferStatus.kNotEnoughSpace:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantReceive');
        this.errorDescription_ = this.i18n('nearbyShareErrorNotEnoughSpace');
        break;
      case TransferStatus.kCancelled:
        this.errorTitle_ = this.i18n('nearbyShareErrorCantReceive');
        this.errorDescription_ = this.i18n('nearbyShareErrorCancelled');
        break;
      case TransferStatus.kFailed:
      case TransferStatus.kMediaUnavailable:
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
        this.errorTitle_ = this.i18n('nearbyShareErrorCantReceive');
        this.errorDescription_ = this.i18n('nearbyShareErrorSomethingWrong');
        break;
      default:
        this.errorTitle_ = null;
        this.errorDescription_ = null;
    }
  }

  private getConnectionTokenString_(): string {
    return this.connectionToken ?
        this.i18n(
            'nearbyShareReceiveConfirmPageConnectionId', this.connectionToken) :
        '';
  }

  /**
   * Returns the URL for the asset that defines a file transfer's animated
   * progress bar.
   */
  private getAnimationUrl_(): string {
    if (this.isJellyEnabled_) {
      return PROGRESS_BAR_URL_JELLY;
    }

    return this.isDarkModeActive_ ? PROGRESS_BAR_URL_DARK :
                                    PROGRESS_BAR_URL_LIGHT;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NearbyShareConfirmPageElement.is]: NearbyShareConfirmPageElement;
  }
}

customElements.define(
    NearbyShareConfirmPageElement.is, NearbyShareConfirmPageElement);
