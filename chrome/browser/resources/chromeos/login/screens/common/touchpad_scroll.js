// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for touchpad scroll screen.
 */

import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_illo_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

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
const TouchpadScrollScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * Enum to represent steps on the touchpad scroll screen.
 * Currently there is only one step, but we still use
 * MultiStepBehavior because it provides implementation of
 * things like processing 'focus-on-show' class
 * @enum {string}
 */
const TouchpadScrollStep = {
  OVERVIEW: 'overview',
};


/**
 * Available user actions.
 * @enum {string}
 */
const UserAction = {
  NEXT: 'next',
  REVERSE: 'update-scroll',
  RETURN: 'return',
};

/**
 * @polymer
 */
class TouchpadScrollScreen extends TouchpadScrollScreenElementBase {
  static get is() {
    return 'touchpad-scroll-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      isReverseScrolling_: {
        type: Boolean,
        value: false,
        observer: 'onCheckChanged_',
      },

      /**
       * Whether the button to return to CHOOBE screen should be shown.
       * @private
       */
      shouldShowReturn_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
    this.resizeobserver_ = new ResizeObserver(() => this.onresize());
  }

  get EXTERNAL_API() {
    return ['setReverseScrolling'];
  }

  get UI_STEPS() {
    return TouchpadScrollStep;
  }

  defaultUIStep() {
    return TouchpadScrollStep.OVERVIEW;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('TouchpadScrollScreen');
    const scrollArea = this.shadowRoot.querySelector('#scrollArea');
    if (scrollArea !== null) {
      this.resizeobserver_.observe(scrollArea);
    }
  }

  onresize() {
    const scrollArea = this.shadowRoot.querySelector('#scrollArea');
    // Removing the margin to set it
    scrollArea.scrollTop = scrollArea.scrollHeight / 2 - 150;
  }

  onBeforeShow(data) {
    this.shouldShowReturn_ = data['shouldShowReturn'];
  }

  /**
   * Set the toggle to the synced
   * scrolling preferences.
   * @param {boolean} isReverseScrolling
   */
  setReverseScrolling(isReverseScrolling) {
    this.isReverseScrolling_ = isReverseScrolling;
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.CHOOBE;
  }

  onCheckChanged_(newValue, oldValue) {
    // Do not forward action to browser during property initialization
    if (oldValue != null) {
      this.userActed([UserAction.REVERSE, newValue]);
    }
  }

  onNextClicked_() {
    this.userActed(UserAction.NEXT);
  }

  onReturnClicked_() {
    this.userActed(UserAction.RETURN);
  }

  getAriaLabelToggleButtons_(locale, title, subtitle) {
    return this.i18nDynamic(locale, title) + '. ' +
        this.i18nDynamic(locale, subtitle);
  }
}

customElements.define(TouchpadScrollScreen.is, TouchpadScrollScreen);
