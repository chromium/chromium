// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-high-visibility-page' component is opened when the
 * user is broadcast in high-visibility mode. The user may cancel to stop high
 * visibility mode at any time.
 */

import 'chrome://resources/ash/common/cr_elements/cr_lottie/cr_lottie.js';
import 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '/shared/nearby_page_template.js';
import '/shared/nearby_shared_icons.html.js';

import {RegisterReceiveSurfaceResult} from '/shared/nearby_share.mojom-webui.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_share_high_visibility_page.html.js';

/**
 * Represents the current error state, if one exists.
 */
enum NearbyVisibilityErrorState {
  TIMED_OUT = 0,
  NO_CONNECTION_MEDIUM = 1,
  TRANSFER_IN_PROGRESS = 2,
  SOMETHING_WRONG = 3,
}

/**
 * The pulse animation asset URL.
 */
const PULSE_ANIMATION_URL = 'nearby_share_pulse_animation.json';

const NearbyShareHighVisibilityPageElementBase = I18nMixin(PolymerElement);

export class NearbyShareHighVisibilityPageElement extends
    NearbyShareHighVisibilityPageElementBase {
  static get is() {
    return 'nearby-share-high-visibility-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      deviceName: {
        notify: true,
        type: String,
        value: 'DEVICE_NAME_NOT_SET',
      },

      /**
       * DOMHighResTimeStamp in milliseconds of when high visibility will be
       * turned off.
       */
      shutoffTimestamp: {
        type: Number,
        value: 0,
      },

      /**
       * Calculated value of remaining seconds of high visibility mode.
       * Initialized to -1 to differentiate it from timed out state.
       */
      remainingTimeInSeconds_: {
        type: Number,
        value: -1,
      },

      registerResult: {
        type: RegisterReceiveSurfaceResult,
        value: null,
      },

      nearbyProcessStopped: {
        type: Boolean,
        value: false,
      },

      startAdvertisingFailed: {
        type: Boolean,
        value: false,
      },

      /**
       * A null |setupState_| indicates that the operation has not yet started.
       */
      errorState_: {
        type: Number,
        value: null,
        computed:
            'computeErrorState_(shutoffTimestamp, remainingTimeInSeconds_,' +
            'registerResult, nearbyProcessStopped, startAdvertisingFailed)',
      },
    };
  }

  deviceName: string;
  nearbyProcessStopped: boolean;
  registerResult: RegisterReceiveSurfaceResult|null;
  shutoffTimestamp: number;
  startAdvertisingFailed: boolean;
  private errorState_: NearbyVisibilityErrorState|null;
  private remainingTimeInSeconds_: number;
  private remainingTimeIntervalId_: number;

  constructor() {
    super();

    this.remainingTimeIntervalId_ = -1;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.calculateRemainingTime_();
    this.remainingTimeIntervalId_ = setInterval(() => {
      this.calculateRemainingTime_();
    }, 1000);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    if (this.remainingTimeIntervalId_ !== -1) {
      clearInterval(this.remainingTimeIntervalId_);
      this.remainingTimeIntervalId_ = -1;
    }
  }

  private highVisibilityTimedOut_(): boolean {
    // High visibility session is timed out only if remaining seconds is 0 AND
    // timestamp is also set.
    return (this.remainingTimeInSeconds_ === 0) &&
        (this.shutoffTimestamp !== 0);
  }

  private computeErrorState_(): NearbyVisibilityErrorState|null {
    if (this.registerResult ===
        RegisterReceiveSurfaceResult.kNoConnectionMedium) {
      return NearbyVisibilityErrorState.NO_CONNECTION_MEDIUM;
    }
    if (this.registerResult ===
        RegisterReceiveSurfaceResult.kTransferInProgress) {
      return NearbyVisibilityErrorState.TRANSFER_IN_PROGRESS;
    }
    if (this.highVisibilityTimedOut_()) {
      return NearbyVisibilityErrorState.TIMED_OUT;
    }
    if (this.registerResult === RegisterReceiveSurfaceResult.kFailure ||
        this.nearbyProcessStopped || this.startAdvertisingFailed) {
      return NearbyVisibilityErrorState.SOMETHING_WRONG;
    }
    return null;
  }

  private getErrorTitle_(): string {
    switch (this.errorState_) {
      case NearbyVisibilityErrorState.TIMED_OUT:
        return this.i18n('nearbyShareErrorTimeOut');
      case NearbyVisibilityErrorState.NO_CONNECTION_MEDIUM:
        return this.i18n('nearbyShareErrorNoConnectionMedium');
      case NearbyVisibilityErrorState.TRANSFER_IN_PROGRESS:
        return this.i18n('nearbyShareErrorTransferInProgressTitle');
      case NearbyVisibilityErrorState.SOMETHING_WRONG:
        return this.i18n('nearbyShareErrorCantReceive');
      default:
        return '';
    }
  }

  private getErrorDescription_(): TrustedHTML|string {
    switch (this.errorState_) {
      case NearbyVisibilityErrorState.TIMED_OUT:
        return this.i18nAdvanced('nearbyShareHighVisibilityTimeoutText');
      case NearbyVisibilityErrorState.NO_CONNECTION_MEDIUM:
        return this.i18n('nearbyShareErrorNoConnectionMediumDescription');
      case NearbyVisibilityErrorState.TRANSFER_IN_PROGRESS:
        return this.i18n('nearbyShareErrorTransferInProgressDescription');
      case NearbyVisibilityErrorState.SOMETHING_WRONG:
        return this.i18n('nearbyShareErrorSomethingWrong');
      default:
        return '';
    }
  }

  private getSubTitle_(): string {
    if (this.remainingTimeInSeconds_ === -1) {
      return '';
    }

    let timeValue = '';
    if (this.remainingTimeInSeconds_ > 60) {
      timeValue = this.i18n(
          'nearbyShareHighVisibilitySubTitleMinutes',
          Math.ceil(this.remainingTimeInSeconds_ / 60));
    } else {
      timeValue = this.i18n(
          'nearbyShareHighVisibilitySubTitleSeconds',
          this.remainingTimeInSeconds_);
    }

    return this.i18n(
        'nearbyShareHighVisibilitySubTitle', this.deviceName, timeValue);
  }

  private calculateRemainingTime_(): void {
    if (this.shutoffTimestamp === 0) {
      return;
    }

    const now = performance.now();
    const remainingTimeInMs =
        this.shutoffTimestamp > now ? this.shutoffTimestamp - now : 0;
    this.remainingTimeInSeconds_ = Math.ceil(remainingTimeInMs / 1000);
  }

  /**
   * Announce the remaining time for screen readers. Only announce once per
   * minute to avoid overwhelming user. Though this gets called once every
   * second, the value returned only changes each minute.
   * @return The alternate page subtitle to be used as an aria-live
   *     announcement for screen readers.
   */
  private getA11yAnnouncedSubTitle_(): string {
    // Skip announcement for 0 seconds left to avoid alerting on time out.
    // There is a separate time out alert shown in the error section.
    if (this.remainingTimeInSeconds_ === 0) {
      return '';
    }
    const remainingMinutes = this.remainingTimeInSeconds_ > 0 ?
        Math.ceil(this.remainingTimeInSeconds_ / 60) :
        5;

    const timeValue =
        this.i18n('nearbyShareHighVisibilitySubTitleMinutes', remainingMinutes);

    return this.i18n(
        'nearbyShareHighVisibilitySubTitle', this.deviceName, timeValue);
  }

  /**
   * Returns the URL for the asset that defines the high visibility page's
   * pulsing background animation.
   */
  private getAnimationUrl_(): string {
    return PULSE_ANIMATION_URL;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NearbyShareHighVisibilityPageElement.is]:
        NearbyShareHighVisibilityPageElement;
  }
}

customElements.define(
    NearbyShareHighVisibilityPageElement.is,
    NearbyShareHighVisibilityPageElement);
