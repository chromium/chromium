// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for touchpad scroll screen.
 */

import '//resources/cr_elements/cr_slider/cr_slider.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/oobe_display_size_selector.js';
import '../../components/oobe_icons.html.js';

import {CrSliderElement} from '//resources/cr_elements/cr_slider/cr_slider.js';
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
const DisplaySizeScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * Enum to represent steps on the display size screen.
 * Currently there is only one step, but we still use
 * MultiStepBehavior because it provides implementation of
 * things like processing 'focus-on-show' class
 * @enum {string}
 */
const DisplaySizeStep = {
  OVERVIEW: 'overview',
};

/**
 * Available user actions.
 * @enum {string}
 */
const UserAction = {
  NEXT: 'next',
  RETURN: 'return',
};

/**
 * @polymer
 */
class DisplaySizeScreen extends DisplaySizeScreenElementBase {
  static get is() {
    return 'display-size-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      shouldShowReturn_: {
        type: Boolean,
        value: false,
      },
    };
  }

  get EXTERNAL_API() {
    return [];
  }

  get UI_STEPS() {
    return DisplaySizeStep;
  }

  defaultUIStep() {
    return DisplaySizeStep.OVERVIEW;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('DisplaySizeScreen');
  }

  onBeforeShow(data) {
    this.$.sizeSelector.init(data['availableSizes'], data['currentSize']);
    this.shouldShowReturn_ = data['shouldShowReturn'];
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.CHOOBE;
  }

  onNextClicked_() {
    this.userActed([UserAction.NEXT, this.$.sizeSelector.getSelectedSize()]);
  }

  onReturnClicked_() {
    this.userActed([UserAction.RETURN, this.$.sizeSelector.getSelectedSize()]);
  }
}

customElements.define(DisplaySizeScreen.is, DisplaySizeScreen);
