// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Update screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '../../components/oobe_cr_lottie.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_carousel.js';
import '../../components/oobe_slide.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const ConsumerUpdateScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

const UNREACHABLE_PERCENT = 1000;
// Thresholds which are used to determine when update status announcement should
// take place. Last element is not reachable to simplify implementation.
const PERCENT_THRESHOLDS = [
  0,
  10,
  20,
  30,
  40,
  50,
  60,
  70,
  80,
  90,
  95,
  98,
  99,
  100,
  UNREACHABLE_PERCENT,
];

/**
 * Enum to represent steps on the consumer update screen.
 * @enum {string}
 */
const ConsumerUpdateStep = {
  CHECKING: 'checking',
  UPDATE: 'update',
  RESTART: 'restart',
  REBOOT: 'reboot',
  CELLULAR: 'cellular',
};


/**
 * Available user actions.
 * @enum {string}
 */
const UserAction = {
  BACK: 'back',
  SKIP: 'skip-consumer-update',
  DECLINE_CELLULAR: 'consumer-update-reject-cellular',
  ACCEPT_CELLULAR: 'consumer-update-accept-cellular',
};

/**
 * @typedef {{
 *   betterUpdatePercent:  HTMLDivElement,
 *   betterUpdateTimeleft:  HTMLDivElement,
 * }}
 */
ConsumerUpdateScreenElementBase.$;

/**
 * @polymer
 */
class ConsumerUpdateScreen extends ConsumerUpdateScreenElementBase {
  static get is() {
    return 'consumer-update-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * True if update is forced.
       */
      isUpdateMandatory: {
        type: Boolean,
        value: true,
      },

      /**
       * Shows battery warning message during Downloading stage.
       */
      showLowBatteryWarning: {
        type: Boolean,
        value: false,
      },

      /**
       * Message like "3% complete".
       */
      updateStatusMessagePercent: {
        type: String,
        value: '',
      },

      /**
       * Message like "About 5 minutes left".
       */
      updateStatusMessageTimeLeft: {
        type: String,
        value: '',
      },

      /**
       * Progress bar percent that is used in BetterUpdate version of the
       * screen.
       */
      betterUpdateProgressValue: {
        type: Number,
        value: 0,
      },

      /**
       * Whether auto-transition is enabled or not.
       */
      autoTransition: {
        type: Boolean,
        value: true,
      },

      /**
       * Index of threshold that has been already achieved.
       */
      thresholdIndex: {
        type: Number,
        value: 0,
      },

    };
  }

  static get observers() {
    return ['playAnimation_(uiStep)'];
  }

  get EXTERNAL_API() {
    return [
      'setIsUpdateMandatory',
      'showLowBatteryWarningMessage',
      'setUpdateState',
      'setUpdateStatus',
      'setAutoTransition',
    ];
  }

  get UI_STEPS() {
    return ConsumerUpdateStep;
  }

  defaultUIStep() {
    return ConsumerUpdateStep.CHECKING;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('ConsumerUpdateScreen');
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  /**
   * Shows or hides skip button while update in progress.
   * @param {boolean} visible Is skip button visible?
   */
  setIsUpdateMandatory(visible) {
    this.isUpdateMandatory = visible;
  }

  /**
   * Decline to use cellular data.
   */
  onDeclineCellularClicked_() {
    this.userActed(UserAction.DECLINE_CELLULAR);
  }

  /**
   * Accept to use cellular data.
   */
  onAcceptCellularClicked_() {
    this.userActed(UserAction.ACCEPT_CELLULAR);
  }

  onSkip_() {
    this.userActed(UserAction.SKIP);
  }

  /**
   * Shows or hides battery warning message.
   * @param {boolean} visible Is message visible?
   */
  showLowBatteryWarningMessage(visible) {
    this.showLowBatteryWarning = visible;
  }

  /**
   * Sets which dialog should be shown.
   * @param {ConsumerUpdateStep} value Current update state.
   */
  setUpdateState(value) {
    this.setUIStep(value);
  }

  /**
   * Sets percent to be shown in progress bar.
   * @param {number} percent Current progress
   * @param {string} messagePercent Message describing current progress.
   * @param {string} messageTimeLeft Message describing time left.
   */
  setUpdateStatus(percent, messagePercent, messageTimeLeft) {
    // Sets aria-live polite on percent and timeleft container every time
    // new threshold has been achieved otherwise do not initiate spoken
    // feedback update by setting aria-live off.
    if (percent >= PERCENT_THRESHOLDS[this.thresholdIndex]) {
      while (percent >= PERCENT_THRESHOLDS[this.thresholdIndex]) {
        this.thresholdIndex = this.thresholdIndex + 1;
      }
      this.$.betterUpdatePercent.setAttribute('aria-live', 'polite');
      this.$.betterUpdateTimeleft.setAttribute('aria-live', 'polite');
    } else {
      this.$.betterUpdateTimeleft.setAttribute('aria-live', 'off');
      this.$.betterUpdatePercent.setAttribute('aria-live', 'off');
    }
    this.betterUpdateProgressValue = percent;
    this.updateStatusMessagePercent = messagePercent;
    this.updateStatusMessageTimeLeft = messageTimeLeft;
  }

  /**
   * Sets whether carousel should auto transit slides.
   */
  setAutoTransition(value) {
    this.autoTransition = value;
  }

  /**
   * Gets whether carousel should auto transit slides.
   * @private
   * @param {ConsumerUpdateStep} step Which UIState is shown now.
   * @param {boolean} autoTransition Is auto transition allowed.
   */
  getAutoTransition_(step, autoTransition) {
    return step == ConsumerUpdateStep.UPDATE && autoTransition;
  }

  onBackClicked_() {
    this.userActed(UserAction.BACK);
  }

  /**
   * @private
   * @param {ConsumerUpdateStep} uiStep which UIState is shown now.
   */
  playAnimation_(uiStep) {
    this.$.checkingAnimation.playing = (uiStep === ConsumerUpdateStep.CHECKING);
  }
}
customElements.define(ConsumerUpdateScreen.is, ConsumerUpdateScreen);
