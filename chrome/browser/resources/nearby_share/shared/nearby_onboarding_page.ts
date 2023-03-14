// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-onboarding-page' component handles the Nearby Share
 * onboarding flow. It is embedded in chrome://os-settings, chrome://settings
 * and as a standalone dialog via chrome://nearby.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './nearby_page_template.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {DeviceNameValidationResult} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyShareOnboardingFinalState, processOnboardingCancelledMetrics, processOnboardingInitiatedMetrics} from './nearby_metrics_logger.js';
import {getTemplate} from './nearby_onboarding_page.html.js';
import {getNearbyShareSettings} from './nearby_share_settings.js';
import {NearbySettings} from './nearby_share_settings_mixin.js';

const ONBOARDING_SPLASH_LIGHT_ICON =
    'nearby-images:nearby-onboarding-splash-light';

const ONBOARDING_SPLASH_DARK_ICON =
    'nearby-images:nearby-onboarding-splash-dark';

export interface NearbyOnboardingPageElement {
  $: {
    deviceName: CrInputElement,
  };
}

const NearbyOnboardingPageElementBase = I18nMixin(PolymerElement);

export class NearbyOnboardingPageElement extends
    NearbyOnboardingPageElementBase {
  static get is() {
    return 'nearby-onboarding-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      settings: {
        type: Object,
      },

      errorMessage: {
        type: String,
        value: '',
      },

      /**
       * Whether the onboarding page is being rendered in dark mode.
       */
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },

    };
  }

  errorMessage: string;
  settings: NearbySettings|null;
  private isDarkModeActive_: boolean;

  override ready(): void {
    super.ready();

    this.addEventListener('next', this.onNext_);
    this.addEventListener('close', this.onClose_);
    this.addEventListener('keydown', this.onKeydown_);
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
  }

  private onNext_(): void {
    this.submitDeviceNameInput_();
  }

  private onClose_(): void {
    processOnboardingCancelledMetrics(
        NearbyShareOnboardingFinalState.DEVICE_NAME_PAGE);
    const onboardingCancelledEvent = new CustomEvent('onboarding-cancelled', {
      bubbles: true,
      composed: true,
    });
    this.dispatchEvent(onboardingCancelledEvent);
  }

  private onKeydown_(e: KeyboardEvent): void {
    e.stopPropagation();
    if (e.key === 'Enter') {
      this.submitDeviceNameInput_();
      e.preventDefault();
    }
  }

  private onViewEnterStart_(): void {
    this.$.deviceName.focus();
    processOnboardingInitiatedMetrics(new URL(document.URL));
  }


  private async onDeviceNameInput_(): Promise<void> {
    const result = await getNearbyShareSettings().validateDeviceName(
        this.$.deviceName.value);
    this.updateErrorMessage_(result.result);
  }

  private async submitDeviceNameInput_(): Promise<void> {
    const result =
        await getNearbyShareSettings().setDeviceName(this.$.deviceName.value);
    this.updateErrorMessage_(result.result);
    if (result.result === DeviceNameValidationResult.kValid) {
      const changePageEvent = new CustomEvent(
          'change-page',
          {bubbles: true, composed: true, detail: {page: 'visibility'}});
      this.dispatchEvent(changePageEvent);
    }
  }

  /**
   * @param validationResult The error status from validating the provided
   *    device name.
   */
  private updateErrorMessage_(validationResult: DeviceNameValidationResult):
      void {
    switch (validationResult) {
      case DeviceNameValidationResult.kErrorEmpty:
        this.errorMessage = this.i18n('nearbyShareDeviceNameEmptyError');
        break;
      case DeviceNameValidationResult.kErrorTooLong:
        this.errorMessage = this.i18n('nearbyShareDeviceNameTooLongError');
        break;
      case DeviceNameValidationResult.kErrorNotValidUtf8:
        this.errorMessage =
            this.i18n('nearbyShareDeviceNameInvalidCharactersError');
        break;
      default:
        this.errorMessage = '';
        break;
    }
  }

  private hasErrorMessage_(errorMessage: string): boolean {
    return errorMessage !== '';
  }

  /**
   * Returns the icon based on Light/Dark mode.
   */
  private getOnboardingSplashIcon_(): string {
    return this.isDarkModeActive_ ? ONBOARDING_SPLASH_DARK_ICON :
                                    ONBOARDING_SPLASH_LIGHT_ICON;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NearbyOnboardingPageElement.is]: NearbyOnboardingPageElement;
  }
}

customElements.define(
    NearbyOnboardingPageElement.is, NearbyOnboardingPageElement);
