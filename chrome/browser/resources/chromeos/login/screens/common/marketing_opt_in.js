// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design marketing
 * opt-in screen.
 */

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/oobe_cr_lottie.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeBackButton} from '../../components/buttons/oobe_back_button.js';
import {OobeIconButton} from '../../components/buttons/oobe_icon_button.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {OOBE_UI_STATE, SCREEN_GAIA_SIGNIN} from '../../components/display_manager_types.js';
import {OobeA11yOption} from '../../components/oobe_a11y_option.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const MarketingScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * Enum to represent each page in the marketing opt in screen.
 * @enum {string}
 */
const MarketingOptInStep = {
  OVERVIEW: 'overview',
  ACCESSIBILITY: 'accessibility',
};

/**
 * @typedef {{
 *   marketingOptInOverviewDialog:  OobeAdaptiveDialog,
 *   chromebookUpdatesOption:  CrToggleElement,
 *   a11yNavButtonToggle:  OobeA11yOption,
 * }}
 */
MarketingScreenElementBase.$;

/**
 * @polymer
 */
class MarketingOptIn extends MarketingScreenElementBase {
  static get is() {
    return 'marketing-opt-in-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Whether the accessibility button is shown. This button is only shown
       * if the gesture EDU screen was shown before the marketing screen.
       */
      isA11ySettingsButtonVisible_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the marketing opt in toggles should be shown, which will be the
       * case only if marketing opt in feature is enabled AND if the current
       * user is a non-managed user. When this is false, the screen will only
       * contain UI related to the tablet mode gestural navigation settings.
       */
      marketingOptInVisible_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether a verbose footer will be shown to the user containing some
       * legal information such as the Google address. Currently shown for
       * Canada only.
       */
      hasLegalFooter_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the device is cloud gaming device, which will
       * alternate different title, subtitle and animation.
       */
      isCloudGamingDevice_: {
        type: Boolean,
        value: false,
      },
    };
  }

  get UI_STEPS() {
    return MarketingOptInStep;
  }

  defaultUIStep() {
    return MarketingOptInStep.OVERVIEW;
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.MARKETING_OPT_IN;
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return ['updateA11ySettingsButtonVisibility',
            'updateA11yNavigationButtonToggle'];
  }

  // clang-format on

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('MarketingOptInScreen');
  }

  /** Shortcut method to control animation */
  setAnimationPlay_(played) {
    this.$.animation.playing = played;
  }

  onBeforeShow(data) {
    this.marketingOptInVisible_ =
        'optInVisibility' in data && data.optInVisibility;
    this.$.chromebookUpdatesOption.checked =
        'optInDefaultState' in data && data.optInDefaultState;
    this.hasLegalFooter_ =
        'legalFooterVisibility' in data && data.legalFooterVisibility;
    this.isCloudGamingDevice_ =
        'cloudGamingDevice' in data && data.cloudGamingDevice;
    this.setAnimationPlay_(true);
    this.$.marketingOptInOverviewDialog.show();
  }

  get defaultControl() {
    return this.$.marketingOptInOverviewDialog;
  }

  /**
   * This is 'on-tap' event handler for 'AcceptAndContinue/Next' buttons.
   * @private
   */
  onGetStarted_() {
    this.setAnimationPlay_(false);
    this.userActed(['get-started', this.$.chromebookUpdatesOption.checked]);
  }

  /**
   * @param {boolean} shown Whether the A11y Settings button should be shown.
   */
  updateA11ySettingsButtonVisibility(shown) {
    this.isA11ySettingsButtonVisible_ = shown;
  }

  /**
   * @param {boolean} enabled Whether the a11y setting for shownig shelf
   * navigation buttons is enabled.
   */
  updateA11yNavigationButtonToggle(enabled) {
    this.$.a11yNavButtonToggle.checked = enabled;
  }

  /**
   * This is the 'on-tap' event handler for the accessibility settings link and
   * for the back button on the accessibility page.
   * @private
   */
  onToggleAccessibilityPage_() {
    if (this.uiStep == MarketingOptInStep.OVERVIEW) {
      this.setUIStep(MarketingOptInStep.ACCESSIBILITY);
      this.setAnimationPlay_(false);
    } else {
      this.setUIStep(MarketingOptInStep.OVERVIEW);
      this.setAnimationPlay_(true);
    }
  }

  /**
   * The 'on-change' event handler for when the a11y navigation button setting
   * is toggled on or off.
   * @private
   */
  onA11yNavButtonsSettingChanged_() {
    this.userActed(
        ['set-a11y-button-enable', this.$.a11yNavButtonToggle.checked]);
  }

  /**
   * Returns the src of the icon.
   * @private
   */
  getIcon_() {
    return this.isCloudGamingDevice_ ? 'oobe-32:game-controller' :
                                       'oobe-32:checkmark';
  }
}

customElements.define(MarketingOptIn.is, MarketingOptIn);
