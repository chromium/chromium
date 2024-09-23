// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design marketing
 * opt-in screen.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/oobe_a11y_option.js';
import '../../components/oobe_cr_lottie.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_icon_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import type {OobeCrLottie} from '../../components/oobe_cr_lottie.js';

import {getTemplate} from './marketing_opt_in.html.js';

const MarketingScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

/**
 * Enum to represent each page in the marketing opt in screen.
 */
enum MarketingOptInStep {
  OVERVIEW = 'overview',
  ACCESSIBILITY = 'accessibility',
}

interface MarketingScreenData {
  optInVisibility: boolean;
  optInDefaultState: boolean;
  legalFooterVisibility: boolean;
  cloudGamingDevice: boolean;
}

export class MarketingOptIn extends MarketingScreenElementBase {
  static get is() {
    return 'marketing-opt-in-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Whether the accessibility button is shown. This button is only shown
       * if the gesture EDU screen was shown before the marketing screen.
       */
      isA11ySettingsButtonVisible: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the marketing opt in toggles should be shown, which will be the
       * case only if marketing opt in feature is enabled AND if the current
       * user is a non-managed user. When this is false, the screen will only
       * contain UI related to the tablet mode gestural navigation settings.
       */
      marketingOptInVisible: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether a verbose footer will be shown to the user containing some
       * legal information such as the Google address. Currently shown for
       * Canada only.
       */
      hasLegalFooter: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the device is cloud gaming device, which will
       * alternate different title, subtitle and animation.
       */
      isCloudGamingDevice: {
        type: Boolean,
        value: false,
      },
    };
  }

  private isA11ySettingsButtonVisible: boolean;
  private marketingOptInVisible: boolean;
  private hasLegalFooter: boolean;
  private isCloudGamingDevice: boolean;

  override get UI_STEPS() {
    return MarketingOptInStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return MarketingOptInStep.OVERVIEW;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.MARKETING_OPT_IN;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'updateA11ySettingsButtonVisibility',
      'updateA11yNavigationButtonToggle',
    ];
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('MarketingOptInScreen');
  }

  /** Shortcut method to control animation */
  private setAnimationPlay(played: boolean): void {
    const animation =
        this.shadowRoot!.querySelector<OobeCrLottie>('#animation');
    if (animation) {
      animation.playing = played;
    }
  }

  /**
   * @param data Screen init payload.
   */
  override onBeforeShow(data: MarketingScreenData) {
    super.onBeforeShow(data);
    this.marketingOptInVisible =
        'optInVisibility' in data && data.optInVisibility;
    this.shadowRoot!
        .querySelector<HTMLInputElement>('#chromebookUpdatesOption')!.checked =
        'optInDefaultState' in data && data.optInDefaultState;
    this.hasLegalFooter =
        'legalFooterVisibility' in data && data.legalFooterVisibility;
    this.isCloudGamingDevice =
        'cloudGamingDevice' in data && data.cloudGamingDevice;
    this.setAnimationPlay(true);
    this.shadowRoot!
        .querySelector<OobeAdaptiveDialog>(
            '#marketingOptInOverviewDialog')!.show();
  }

  override get defaultControl(): HTMLElement|null {
    return this.shadowRoot!.querySelector<HTMLElement>(
        '#marketingOptInOverviewDialog')!;
  }

  /**
   * This is 'on-click' event handler for 'AcceptAndContinue/Next' buttons.
   */
  private onGetStarted(): void {
    this.setAnimationPlay(false);
    this.userActed([
      'get-started',
      this.shadowRoot!
          .querySelector<HTMLInputElement>('#chromebookUpdatesOption')!.checked,
    ]);
  }

  /**
   * @param shown Whether the A11y Settings button should be shown.
   */
  updateA11ySettingsButtonVisibility(shown: boolean): void {
    this.isA11ySettingsButtonVisible = shown;
  }

  /**
   * @param enabled Whether the a11y setting for shownig shelf
   * navigation buttons is enabled.
   */
  updateA11yNavigationButtonToggle(enabled: boolean): void {
    this.shadowRoot!.querySelector<HTMLInputElement>(
                        '#a11yNavButtonToggle')!.checked = enabled;
  }

  /**
   * This is the 'on-click' event handler for the accessibility settings link
   * and for the back button on the accessibility page.
   */
  private onToggleAccessibilityPage(): void {
    if (this.uiStep === MarketingOptInStep.OVERVIEW) {
      this.setUIStep(MarketingOptInStep.ACCESSIBILITY);
      this.setAnimationPlay(false);
    } else {
      this.setUIStep(MarketingOptInStep.OVERVIEW);
      this.setAnimationPlay(true);
    }
  }

  /**
   * The 'on-change' event handler for when the a11y navigation button setting
   * is toggled on or off.
   */
  private onA11yNavButtonsSettingChanged(): void {
    this.userActed([
      'set-a11y-button-enable',
      this.shadowRoot!.querySelector<HTMLInputElement>(
                          '#a11yNavButtonToggle')!.checked,
    ]);
  }

  /**
   * Returns the src of the icon.
   */
  private getIcon(): string {
    return this.isCloudGamingDevice ? 'oobe-32:game-controller' :
                                      'oobe-32:checkmark';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [MarketingOptIn.is]: MarketingOptIn;
  }
}

customElements.define(MarketingOptIn.is, MarketingOptIn);
