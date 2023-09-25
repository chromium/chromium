// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/dialogs/oobe_loading_dialog.js';


import { assert } from '//resources/ash/common/assert.js';
import { html, mixinBehaviors, PolymerElement } from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';


import { LoginScreenBehavior, LoginScreenBehaviorInterface } from '../../components/behaviors/login_screen_behavior.js';
import { MultiStepBehavior, MultiStepBehaviorInterface } from '../../components/behaviors/multi_step_behavior.js';
import { OobeI18nBehavior, OobeI18nBehaviorInterface } from '../../components/behaviors/oobe_i18n_behavior.js';



/**
 * UI mode for the dialog.
 * @enum {string}
 */
const DialogMode = {
  LOADING: 'loading',
};


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const OnlineAuthenticationScreenElementBase = mixinBehaviors(
  [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);


/**
 * @polymer
 */
class OnlineAuthenticationScreenElement extends OnlineAuthenticationScreenElementBase {
  static get is() {
    return 'online-authentication-screen-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
    };
  }

  get EXTERNAL_API() {
    return [];
  }

  defaultUIStep() {
    return DialogMode.LOADING;
  }

  get UI_STEPS() {
    return DialogMode;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('OnlineAuthenticationScreen');
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload
   */
  onBeforeShow() {
  }
}

customElements.define(OnlineAuthenticationScreenElement.is, OnlineAuthenticationScreenElement);
