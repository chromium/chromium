// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-onboarding-one-page' component handles the Nearby
 * Share onboarding flow. It is embedded in chrome://os-settings,
 * chrome://settings and as a standalone dialog via chrome://nearby.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './nearby_page_template.js';
// <if expr='chromeos_ash'>
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';

// </if>

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {DeviceNameValidationResult, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyShareOnboardingFinalState, processOnePageOnboardingCancelledMetrics, processOnePageOnboardingCompleteMetrics, processOnePageOnboardingInitiatedMetrics, processOnePageOnboardingVisibilityButtonOnInitialPageClickedMetrics} from './nearby_metrics_logger.js';
import {getTemplate} from './nearby_onboarding_one_page.html.js';
import {getNearbyShareSettings} from './nearby_share_settings.js';
import {NearbySettings} from './nearby_share_settings_mixin.js';

const ONE_PAGE_ONBOARDING_SPLASH_LIGHT_ICON =
    'nearby-images:nearby-onboarding-splash-light';

const ONE_PAGE_ONBOARDING_SPLASH_DARK_ICON =
    'nearby-images:nearby-onboarding-splash-dark';

export interface NearbyOnboardingOnePageElement {
  $: {
    deviceName: CrInputElement,
  };
}

const NearbyOnboardingOnePageElementBase = I18nMixin(PolymerElement);

export class NearbyOnboardingOnePageElement extends
    NearbyOnboardingOnePageElementBase {
  static get is() {
    return 'nearby-onboarding-one-page' as const;
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
    this.finishOnboarding_();
  }

  private onClose_(): void {
    processOnePageOnboardingCancelledMetrics(
        NearbyShareOnboardingFinalState.INITIAL_PAGE);

    const onboardingCancelledEvent = new CustomEvent('onboarding-cancelled', {
      bubbles: true,
      composed: true,
    });
    this.dispatchEvent(onboardingCancelledEvent);
  }

  private onKeydown_(e: KeyboardEvent): void {
    e.stopPropagation();
    if (e.key === 'Enter') {
      this.finishOnboarding_();
      e.preventDefault();
    }
  }

  private onViewEnterStart_(): void {
    this.$.deviceName.focus();
    processOnePageOnboardingInitiatedMetrics(new URL(document.URL));
  }

  private async onDeviceNameInput_(): Promise<void> {
    const result = await getNearbyShareSettings().validateDeviceName(
        this.$.deviceName.value);
    this.updateErrorMessage_(result.result);
  }

  private async finishOnboarding_(): Promise<void> {
    const result =
        await getNearbyShareSettings().setDeviceName(this.$.deviceName.value);

    this.updateErrorMessage_(result.result);
    if (result.result === DeviceNameValidationResult.kValid) {
      /**
       * TODO(crbug.com/1265562): remove this line once the old onboarding
       * is deprecated and default visibility is changed in
       * nearby_share_prefs.cc:kNearbySharingBackgroundVisibilityName
       */
      this.set('settings.visibility', this.getDefaultVisibility_());
      this.set('settings.isOnboardingComplete', true);
      this.set('settings.enabled', true);
      processOnePageOnboardingCompleteMetrics(
          NearbyShareOnboardingFinalState.INITIAL_PAGE,
          this.getDefaultVisibility_());
      const onboardingCompleteEvent = new CustomEvent('onboarding-complete', {
        bubbles: true,
        composed: true,
      });
      this.dispatchEvent(onboardingCompleteEvent);
    }
  }

  /**
   * Switch to visibility selection page when the button is clicked
   */
  private switchToVisibilitySelectionView_(): void {
    /**
     * TODO(crbug.com/1265562): remove this line once the old onboarding is
     * deprecated and default visibility is changed in
     * nearby_share_prefs.cc:kNearbySharingBackgroundVisibilityName
     */
    this.set('settings.visibility', this.getDefaultVisibility_());
    processOnePageOnboardingVisibilityButtonOnInitialPageClickedMetrics();

    const changePageEvent = new CustomEvent(
        'change-page',
        {bubbles: true, composed: true, detail: {page: 'visibility'}});
    this.dispatchEvent(changePageEvent);
  }

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
    return this.isDarkModeActive_ ? ONE_PAGE_ONBOARDING_SPLASH_DARK_ICON :
                                    ONE_PAGE_ONBOARDING_SPLASH_LIGHT_ICON;
  }

  /**
   * Temporary workaround to set default visibility. Changing the
   * kNearbySharingBackgroundVisibilityName in nearby_share_prefs.cc results in
   * setting visibility selection to 'all contacts' in nearby_visibility_page in
   * existing onboarding workflow.
   *
   * TODO(crbug.com/1265562): remove this function once the old onboarding is
   * deprecated and default visibility is changed in
   * nearby_share_prefs.cc:kNearbySharingBackgroundVisibilityName
   */
  private getDefaultVisibility_(): Visibility|null {
    if (this.settings!.visibility === Visibility.kUnknown) {
      return Visibility.kAllContacts;
    }
    return this.settings!.visibility;
  }

  private getVisibilitySelectionButtonText_(): string {
    const visibility = this.getDefaultVisibility_();
    switch (visibility) {
      case Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAll');
      case Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySome');
      case Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNone');
      default:
        return this.i18n('nearbyShareContactVisibilityAll');
    }
  }

  private getVisibilitySelectionButtonIcon_(): string {
    const visibility = this.getDefaultVisibility_();
    switch (visibility) {
      case Visibility.kAllContacts:
        return 'contact-all';
      case Visibility.kSelectedContacts:
        return 'contact-group';
      case Visibility.kNoOne:
        return 'visibility-off';
      default:
        return 'contact-all';
    }
  }

  /**
   * TODO(crbug.com/1265562): Add strings for other modes and switch based on
   * default visibility selection
   */
  private getVisibilitySelectionButtonHelpText_(): string {
    return this.i18n(
        'nearbyShareOnboardingPageDeviceVisibilityHelpAllContacts');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NearbyOnboardingOnePageElement.is]: NearbyOnboardingOnePageElement;
  }
}

customElements.define(
    NearbyOnboardingOnePageElement.is, NearbyOnboardingOnePageElement);
